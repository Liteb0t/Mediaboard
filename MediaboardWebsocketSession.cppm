// Copyright (c) 2026, Fuze.page
// Fuze Human-oriented License v1
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
			closeTrack(own_peer_ptr.value()->desktop_audio_track);
			closeTrack(own_peer_ptr.value()->mic_track);
			if (own_peer_ptr.value()->connection)
				own_peer_ptr.value()->connection->close();
			if (tracking_room_ptr) {
				tracking_room_ptr.value()->peers.erase(own_connection_id);
				tracking_room_ptr.value()->broadcastPeerList(own_connection_id, Room::ACTION::LEAVE);
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

	rtc::IceServer::RelayType getRelayType(const Mediaboard::IceServer& server) {
		if (server.transport == "UDP") {
			return rtc::IceServer::RelayType::TurnUdp;
		}
		else if (server.transport == "TCP") {
			return rtc::IceServer::RelayType::TurnTcp;
		}
		else if (server.transport == "TLS") {
			return rtc::IceServer::RelayType::TurnTls;
		}
		else {
			std::println(std::cerr, "Error: transport {} not recignised for server {}:{}", server.transport, server.hostname, server.port);
			return rtc::IceServer::RelayType::TurnUdp;
		}
	}
	rtc::Configuration getRtcConfig() {
		rtc::Configuration config;
		for (const Mediaboard::IceServer& server : getState()->getIceServers()) {
			auto credentials = server.getCredentialForClient(getClient());
			rtc::IceServer ice_server_config(server.hostname, server.port, credentials.username, credentials.credential, getRelayType(server));
			config.iceServers.emplace_back(ice_server_config);
		}
		return config;
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
				auto room = std::make_shared<Room>(new_room_id/*, this->getExecutor()*/);
				this->tracking_room_ptr = room;
				board.value()->rooms.emplace(new_room_id, room);
				this->send({
					{"type", "create_room_response"},
					{"payload", {{"room_id", new_room_id}}}
				});
			}
			else if (request_type == "webrtc_relay_added_answer") {
				std::string sdp = buffer_as_json.at("payload").at("description").at("sdp").as_string().c_str();
				std::string type = buffer_as_json.at("payload").at("description").at("type").as_string().c_str();
				std::string relay_name = buffer_as_json.at("payload").at("relay_name").as_string().c_str();
				if (!own_peer_ptr)
					throw "No WebRTC peer associated with this session";
				own_peer_ptr.value()->connection->setRemoteDescription(rtc::Description(sdp, type));
				tracking_room_ptr.value()->addRelaySlotDependingOnMid(relay_name, own_peer_ptr.value());
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
				own_peer_ptr.value()->connection = std::make_shared<rtc::PeerConnection>(getRtcConfig());


				bool client_has_permission_to_share_media = board.value()->clientHasPermission(this->getClient(), static_cast<int>(PERMISSION::ROOM_SHARE_MEDIA));

				///////////// RECEIVING /////////////
				tracking_room_ptr.value()->addReceivingRelayTracks(own_connection_id, own_peer_ptr.value());

				// [AI glasnost] this section assisted by Claude Sonnet 5
				own_peer_ptr.value()->connection->onGatheringStateChange([this, client_has_permission_to_share_media](rtc::PeerConnection::GatheringState state) {
					std::cout << "Gathering State: " << state << std::endl;
					if (state == rtc::PeerConnection::GatheringState::Complete) {
						auto description = this->own_peer_ptr.value()->connection->localDescription();
						this->send({
							{"type", "webrtc_room_connect"},
							{"payload", {
								{"connection_id", own_connection_id},
								{"can_share_media", client_has_permission_to_share_media},
								{"description", {
									{"type", description->typeString()},
									{"sdp", std::string(description.value())}
								}},
								{"ice_servers", this->getState()->getIceCredentialsForClient(getClient())}
							}}
						});
					}
				});

				////////////// SENDING ////////////////
				if (client_has_permission_to_share_media) {
					{
						rtc::Description::Video media(VIDEO_MID, rtc::Description::Direction::RecvOnly);
						// Idealy H264 would be used because it's the superior codec [source: it just is, ok?]
						// but, for compatibility reasons (god damn it Firefox) Vp8 is used instead
						media.addVP8Codec(96);
						media.setBitrate(3000); // Request 3Mbps (Browsers do not encode more than 2.5MBps from a webcam)
						own_peer_ptr.value()->video_sending_track = own_peer_ptr.value()->connection->addTrack(media);
					}
					own_peer_ptr.value()->video_sending_track->setMediaHandler(std::make_shared<rtc::RtcpReceivingSession>());
					own_peer_ptr.value()->video_sending_track->onMessage([this](rtc::binary message) {
						auto room = this->tracking_room_ptr.value();
						Relay* relay = this->own_peer_ptr.value()->getRelayFromString(VIDEO_MID).value();
						if (!this->own_peer_ptr.value()->video_sharing_enabled)
							return;
						if (!relay->initialized) {
							std::println("Initialising video relay!");
							relay->initialized = true;
							room->addRelaySlots(this->own_connection_id, VIDEO_MID);
						}
						room->sendMessageToRelays(own_connection_id, std::move(message), VIDEO_MID);
					}, nullptr);

					// mic audio
					// TODO side effects: this->tracking_room_ptr.value();, this->own_connection_id
					rtc::Description::Audio mic_media(MIC_AUDIO_MID, rtc::Description::Direction::RecvOnly);
					mic_media.addOpusCodec(111);
					own_peer_ptr.value()->mic_track = own_peer_ptr.value()->connection->addTrack(mic_media);
					own_peer_ptr.value()->mic_track->onMessage([this](rtc::binary message) {
						auto room = this->tracking_room_ptr.value();
						Relay* relay = this->own_peer_ptr.value()->getRelayFromString(MIC_AUDIO_MID).value();
						if (!this->own_peer_ptr.value()->mic_sharing_enabled)
							return;
						if (!relay->initialized) {
							std::println("Initialising mic relay!");
							relay->initialized = true;
							room->addRelaySlots(this->own_connection_id, MIC_AUDIO_MID);
						}
						room->sendMessageToRelays(own_connection_id, std::move(message), MIC_AUDIO_MID);
					}, nullptr);

					rtc::Description::Audio desktop_audio_media(DESKTOP_AUDIO_MID, rtc::Description::Direction::RecvOnly);
					desktop_audio_media.addOpusCodec(111);
					own_peer_ptr.value()->desktop_audio_track = own_peer_ptr.value()->connection->addTrack(desktop_audio_media);
					own_peer_ptr.value()->desktop_audio_track->onMessage([this](rtc::binary message) {
						auto room = this->tracking_room_ptr.value();
						Relay* relay = this->own_peer_ptr.value()->getRelayFromString(DESKTOP_AUDIO_MID).value();
						if (!this->own_peer_ptr.value()->desktop_audio_sharing_enabled)
							return;
						if (!relay->initialized) {
							std::println("Initialising desktop_audio relay!");
							relay->initialized = true;
							room->addRelaySlots(this->own_connection_id, DESKTOP_AUDIO_MID);
						}
						room->sendMessageToRelays(own_connection_id, std::move(message), DESKTOP_AUDIO_MID);
					}, nullptr);
				}
				else {
					// fixes "No DataChannel or Track to negotiate".
					own_peer_ptr.value()->keepalive_channel = own_peer_ptr.value()->connection->createDataChannel("keepalive");
				}

				own_peer_ptr.value()->connection->setLocalDescription();
				// [AI glasnost] end AI-assisted section
				this->tracking_room_ptr.value()->peers.emplace(own_connection_id, own_peer_ptr.value());
				this->tracking_room_ptr.value()->broadcastPeerList(own_connection_id, Room::ACTION::JOIN);
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
			}
			else if (request_type == "webrtc_sharing_status_update") {
				if (!own_peer_ptr)
					throw "No WebRTC peer associated with this session";
				own_peer_ptr.value()->desktop_audio_sharing_enabled = buffer_as_json.at("payload").at("desktop_audio_sharing_enabled").as_bool();
				own_peer_ptr.value()->mic_sharing_enabled = buffer_as_json.at("payload").at("mic_sharing_enabled").as_bool();
				own_peer_ptr.value()->video_sharing_enabled = buffer_as_json.at("payload").at("video_sharing_enabled").as_bool();
				this->tracking_room_ptr.value()->broadcastPeerList(this->own_connection_id, Room::ACTION::UPDATE);
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
