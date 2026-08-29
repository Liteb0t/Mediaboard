module;
#ifdef WITH_WEBRTC
#include <rtc/rtcpreceivingsession.hpp>
#include <rtc/peerconnection.hpp>
#include <rtc/track.hpp>
#endif
#include <boost/asio.hpp>
#include <boost/json.hpp>
#include <iostream>
#include <memory>
#include <print>
#include <stdexcept>
export module Mediaboard.MediaboardWebsocketSession;

import FuzeHttp.PermissionObject;
import FuzeHttp.State;
import Mediaboard.Board;
#ifdef WITH_WEBRTC
import Mediaboard.Room;
#endif
import Mediaboard.Permission;
import Mediaboard.State;

export namespace Mediaboard {
class WebsocketSession : public FuzeHttp::WebsocketSession {
public:
	WebsocketSession(boost::asio::ip::tcp::socket&& socket, FuzeHttp::StateBase* state) : FuzeHttp::WebsocketSession(std::move(socket), state) {}
#ifdef WITH_WEBRTC
	~WebsocketSession() {
		if (own_peer_ptr) {
			auto closeTrack = [](std::shared_ptr<rtc::Track> track) {
				if (track) {
					track->onMessage(nullptr, nullptr); // detach callback before closing
					track->close();
				}
			};
			own_peer_ptr.value()->send_message = nullptr;
			closeTrack(own_peer_ptr.value()->video_sending_track);
			closeTrack(own_peer_ptr.value()->video_receiving_track);
			closeTrack(own_peer_ptr.value()->desktop_audio_track);
			closeTrack(own_peer_ptr.value()->mic_track);
			if (own_peer_ptr.value()->connection)
				own_peer_ptr.value()->connection->close();
			if (tracking_room_ptr) {
				tracking_room_ptr.value()->peers.erase(own_connection_id);
				broadcastPeerList();
			}
		}
	}
#endif
private:
	State* getState() const {
		return (State*)this->state_; // base FuzeHttp state must be casted to Mediaboard state
	}
	int tracking_board = -1; // using pointer could segfault bringing down the whole server, so avoid raw pointers unless its lifetime is within a function
	int tracking_thread = -1;
#ifdef WITH_WEBRTC
	std::optional<std::shared_ptr<Room>> tracking_room_ptr;
	int own_connection_id;
	std::optional<std::shared_ptr<RtcPeer>> own_peer_ptr;
	static const rtc::SSRC targetSSRC = 42;
	void addAudioRelayTrack(std::shared_ptr<RtcPeer> receiver_peer, int sender_connection_id) {
		unsigned int relay_ssrc = tracking_room_ptr.value()->ssrc_counter++;
		rtc::Description::Audio relay_media(std::format("audio_relay_{}", sender_connection_id), rtc::Description::Direction::SendOnly);
		relay_media.addOpusCodec(111);
		relay_media.addSSRC(relay_ssrc, std::format("audio_relay_{}", sender_connection_id));

		auto relay_track = receiver_peer->connection->addTrack(relay_media);
		relay_track->onMessage([this](rtc::binary message) {}, nullptr); // fixes "no receive callback" warning
		receiver_peer->audio_relays.emplace(sender_connection_id, AudioRelaySlot{relay_track, relay_ssrc});

	}
	void addAudioRelaySlot(std::shared_ptr<RtcPeer> receiver_peer, int sender_connection_id) {
		if (receiver_peer->renegotiation_in_flight) {
			receiver_peer->pending_audio_relay_additions.push_back(sender_connection_id);
			return;
		}
		receiver_peer->renegotiation_in_flight = true;
		addAudioRelayTrack(receiver_peer, sender_connection_id);

		// FuzeHttp::WebsocketSession* receiver_session = static_cast<FuzeHttp::WebsocketSession*>(receiver_peer->owner_session);
		receiver_peer->connection->onLocalDescription([receiver_peer, sender_connection_id](rtc::Description description) {
			if (description.type() != rtc::Description::Type::Offer)
				return; // ignore the answer echo, if any
			receiver_peer->send_message({
				{"type", "webrtc_audio_added"},
				{"payload", {
					{"sender_connection_id", sender_connection_id},
					{"description", {
						{"type", description.typeString()},
						{"sdp", std::string(description)}
					}}
				}}
			});
		});
		receiver_peer->connection->setLocalDescription();
	}

	void sendMessageToMicRelays(rtc::binary&& message) {
		for (auto& [peer_id, weak_peer] : tracking_room_ptr.value()->peers) {
			if (peer_id == own_connection_id) continue;
			if (auto strong_peer = weak_peer.lock()) {
				if (auto it = strong_peer->audio_relays.find(own_connection_id); it != strong_peer->audio_relays.end() && it->second.track->isOpen()) {
					auto rtp = reinterpret_cast<rtc::RtpHeader*>(message.data());
					rtp->setSsrc(it->second.ssrc); // it->second is AudioRelaySlot
					it->second.track->send(message);
				}
			}
		}
	}
	void broadcastPeerList() {
		boost::json::object data_to_send = {
			{"type", "peers_updated"},
			{"payload", {{"peers", tracking_room_ptr.value()->asJson().at("peers")}}}
		};
		for (auto& weak_peer : tracking_room_ptr.value()->peers) {
			if (auto strong_peer = weak_peer.second.lock())
				strong_peer->send_message(data_to_send);
		}
	}
#endif
	void readEvent(std::string buffer_data) override {
		try {
			std::cout << buffer_data << std::endl;
			boost::json::object buffer_as_json = boost::json::parse(buffer_data).as_object();

			if (!buffer_as_json.contains("type"))
				throw std::runtime_error("'type' field is missing");
			std::string request_type = buffer_as_json["type"].as_string().c_str();
			if (request_type == "listen_to_thread") {
				int thread_id = buffer_as_json.at("thread_id").as_int64();
				std::string board_slug = buffer_as_json.at("board").as_string().c_str();
				std::optional<Board*> board = getState()->getBoardIfExistsAndClientHasReadPermission(board_slug, getClient());
				if (!board)
					throw std::format("Board '{}' either doesn't exist, or client lacks permission to access it.", board_slug);
				this->tracking_board = board.value()->getId();
				if (!board.value()->threadExists(thread_id))
					std::cout << "Warning: thread " << thread_id << " does not exist" << std::endl;
				if (!board.value()->getThread(thread_id)->clientHasPermission(this->getClient(), static_cast<int>(PERMISSION::VIEW_THREAD)))
					throw std::runtime_error("Client does not have VIEW_THREAD permission");
				this->tracking_thread = thread_id;
				board.value()->addListenerToThread(this, thread_id);
			}
#ifdef WITH_WEBRTC
			else if (request_type == "create_room") {
				std::string board_slug = buffer_as_json.at("board").as_string().c_str();
				std::optional<Board*> board = getState()->getBoardIfExistsAndClientHasReadPermission(board_slug, getClient());
				if (!board)
					throw std::format("Board '{}' either doesn't exist, or client lacks permission to access it.", board_slug);
				// int new_room_id = board.value()->createRoom(this);

				int new_room_id = board.value()->room_id_seq++;
				auto room = std::make_shared<Room>(new_room_id);
				this->tracking_room_ptr = room;
				board.value()->rooms.emplace(new_room_id, room);
				this->send({
					{"type", "create_room_response"},
					{"payload", {{"room_id", new_room_id}}}
				});
			}
			else if (request_type == "webrtc_audio_added_answer") {
				std::string sdp = buffer_as_json.at("payload").at("description").at("sdp").as_string().c_str();
				std::string type = buffer_as_json.at("payload").at("description").at("type").as_string().c_str();
				if (!own_peer_ptr)
					throw "No WebRTC peer associated with this session";
				own_peer_ptr.value()->connection->setRemoteDescription(rtc::Description(sdp, type));
				own_peer_ptr.value()->renegotiation_in_flight = false;
				if (!own_peer_ptr.value()->pending_audio_relay_additions.empty()) {
					int next = own_peer_ptr.value()->pending_audio_relay_additions.front();
					own_peer_ptr.value()->pending_audio_relay_additions.pop_front();
					addAudioRelaySlot(own_peer_ptr.value(), next);
				}
			}
			else if (request_type == "webrtc_room_connect_request") {
				std::string board_slug = buffer_as_json.at("board").as_string().c_str();
				int room_id = buffer_as_json.at("room").as_int64();
				std::optional<Board*> board = getState()->getBoardIfExistsAndClientHasReadPermission(board_slug, getClient());
				if (!board)
					throw std::format("Board '{}' either doesn't exist, or client lacks permission to access it.", board_slug);
				this->tracking_board = board.value()->getId();
				this->tracking_room_ptr = board.value()->getSharedRoomIfExists(room_id); // TODO check client permission
				if (!tracking_room_ptr)
					throw std::format("Room '{}' either doesn't exist, or client lacks permission to access it.", room_id);

				this->own_peer_ptr = std::make_shared<RtcPeer>();
				this->own_connection_id = tracking_room_ptr.value()->connection_id_counter++;
				own_peer_ptr.value()->client_id = getClient() ? getClient()->id : -1;
				own_peer_ptr.value()->send_message = [this](boost::json::object payload) { this->send(std::move(payload)); };
				own_peer_ptr.value()->connection = std::make_shared<rtc::PeerConnection>();

				///////////// RECEIVING /////////////
				for (auto& [sender_id, weak_sender] : tracking_room_ptr.value()->peers) {
					if (sender_id == own_connection_id) continue;
					auto sender_peer = weak_sender.lock();
					if (sender_peer && sender_peer->mic_relay_initialized) {
						addAudioRelayTrack(own_peer_ptr.value(), sender_id);
					}
				}

				own_peer_ptr.value()->connection->onStateChange([](rtc::PeerConnection::State state) {
					std::cout << "State: " << state << std::endl;
				});
				own_peer_ptr.value()->connection->onGatheringStateChange([this](rtc::PeerConnection::GatheringState state) {
					std::cout << "Gathering State: " << state << std::endl;
					if (state == rtc::PeerConnection::GatheringState::Complete) {
						auto description = this->own_peer_ptr.value()->connection->localDescription();
						this->send({
							{"type", "webrtc_room_connect"},
							{"payload", {
								{"connection_id", own_connection_id},
								{"description", {
									{"type", description->typeString()},
									{"sdp", std::string(description.value())}
								}}
							}}
						});
					}
				});
				{
					rtc::Description::Video media("video_relay", rtc::Description::Direction::SendOnly);
					media.addVP8Codec(96);
					media.setBitrate(3000);
					media.addSSRC(targetSSRC, "video_sending", "video_sending", "video_sending");

					own_peer_ptr.value()->video_receiving_track = own_peer_ptr.value()->connection->addTrack(media);

					own_peer_ptr.value()->video_receiving_track->onMessage([](rtc::binary var) {}, nullptr);
				}


				////////////// SENDING ////////////////
				{
					rtc::Description::Video media("video_sending", rtc::Description::Direction::RecvOnly);
					// Idealy H264 would be used because it's the superior codec [source: it just is, ok?]
					// but, for compatibility reasons (god damn it Firefox) Vp8 is used instead
					media.addVP8Codec(96);
					media.setBitrate(3000); // Request 3Mbps (Browsers do not encode more than 2.5MBps from a webcam)
					own_peer_ptr.value()->video_sending_track = own_peer_ptr.value()->connection->addTrack(media);
				}
				own_peer_ptr.value()->video_sending_track->setMediaHandler(std::make_shared<rtc::RtcpReceivingSession>());
				own_peer_ptr.value()->video_sending_track->onMessage(
					[this](rtc::binary message) {
						if (!this->own_peer_ptr.value()->video_sharing_enabled)
							return;
						// This is an RTP packet
						auto rtp = reinterpret_cast<rtc::RtpHeader *>(message.data());
						rtp->setSsrc(targetSSRC);
						auto board = this->getState()->getBoardIfExists(this->tracking_board);
						if (!board)
							throw "board no longer exists";
						for (auto peer : this->tracking_room_ptr.value()->peers) {
							if (auto peer_ptr = peer.second.lock()) {
								if (peer_ptr->video_receiving_track != nullptr && peer_ptr->video_receiving_track->isOpen()) {
									peer_ptr->video_receiving_track->send(message);
								}
							}
						}
					},
					nullptr);


				// mic audio
				rtc::Description::Audio mic_media("mic_audio", rtc::Description::Direction::RecvOnly);
				mic_media.addOpusCodec(111);
				own_peer_ptr.value()->mic_track = own_peer_ptr.value()->connection->addTrack(mic_media);
				own_peer_ptr.value()->mic_track->onMessage([this](rtc::binary message) {
					if (!this->own_peer_ptr.value()->mic_sharing_enabled)
						return;
					if (!this->own_peer_ptr.value()->mic_relay_initialized) {
						std::println("Initialising mic relay!");
						this->own_peer_ptr.value()->mic_relay_initialized = true;
						auto room = this->tracking_room_ptr.value();
						for (auto& [peer_id, weak_peer] : room->peers) {
							if (peer_id == own_connection_id) continue;
							if (auto strong_peer = weak_peer.lock())
								addAudioRelaySlot(strong_peer, own_connection_id);
						}
					}
					sendMessageToMicRelays(std::move(message));
				}, nullptr);

				rtc::Description::Audio desktop_media("desktop_audio", rtc::Description::Direction::RecvOnly);
				desktop_media.addOpusCodec(111);
				own_peer_ptr.value()->desktop_audio_track = own_peer_ptr.value()->connection->addTrack(desktop_media);
				own_peer_ptr.value()->desktop_audio_track->onMessage([this](rtc::binary message) {
					// TODO implement same as mic track
				}, nullptr);

				own_peer_ptr.value()->connection->setLocalDescription();
				this->tracking_room_ptr.value()->peers.emplace(own_connection_id, own_peer_ptr.value());
				broadcastPeerList();
			}
			else if (request_type == "webrtc_room_connect_answer") {
				std::optional<Board*> board = getState()->getBoardIfExistsAndClientHasReadPermission(tracking_board, getClient());
				if (!board)
					throw std::format("Board '{}' either doesn't exist, or client lacks permission to access it.", tracking_board);
				if (!tracking_room_ptr)
					throw "Client is not connected to a room";
				// int sender_connection_id = buffer_as_json.at("payload").at("sender_connection_id").as_int64();
				std::string sdp  = buffer_as_json.at("payload").at("description").at("sdp" ).as_string().c_str();
				std::string type = buffer_as_json.at("payload").at("description").at("type").as_string().c_str();
				rtc::Description answer(sdp, type);
				this->own_peer_ptr.value()->connection->setRemoteDescription(answer);
				// sending keyframe ignored when video sender is not there
				for (auto& weak_peer : tracking_room_ptr.value()->peers) {
					if (auto strong_peer = weak_peer.second.lock()) {
						if (strong_peer->video_sending_track->isOpen() && strong_peer->video_sharing_enabled) {
							strong_peer->video_sending_track->requestKeyframe();
							break;
						}
					}
				}
			}
			else if (request_type == "webrtc_sharing_status_update") {
				if (!own_peer_ptr)
					throw "No WebRTC peer associated with this session";
				own_peer_ptr.value()->mic_sharing_enabled = buffer_as_json.at("payload").at("mic_sharing_enabled").as_bool();
				own_peer_ptr.value()->video_sharing_enabled = buffer_as_json.at("payload").at("video_sharing_enabled").as_bool();
				broadcastPeerList();
			}
#endif
			else {
				throw "request_type " + request_type + " not recognised";
			}
		}
		catch (const std::exception& e) {
			returnError(e.what());
		}
		catch (const char* message) {
			returnError(message);
		}
		catch (const std::string& message) {
			returnError(message);
		}
	}
	void returnError(const std::string& message) {
		std::println(std::cerr, "[MediaboardWebsocketSession] {}", message);
		this->send({{"type", "error"}, {"error_message", message}});
	}
};
}
