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
import Mediaboard.Permission;
import Mediaboard.State;

export namespace Mediaboard {
class WebsocketSession : public FuzeHttp::WebsocketSession {
public:
	WebsocketSession(boost::asio::ip::tcp::socket&& socket, FuzeHttp::StateBase* state) : FuzeHttp::WebsocketSession(std::move(socket), state) {}
private:
	// std::optional<std::shared_ptr<Receiver>> webrtc_receiver;
	int tracking_board = -1; // using pointer could segfault bringing down the whole server, so we avoid raw pointers unless its lifetime is within a function
	std::optional<std::shared_ptr<Room>> tracking_room_ptr;
	int tracking_thread = -1;
#ifdef WITH_WEBRTC
	static const rtc::SSRC targetSSRC = 42;
#endif
	State* getState() const {
		return (State*)this->state_; // base FuzeHttp state must be casted to Mediaboard state
	}
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
			else if (request_type == "connect_to_room") {
				std::string board_slug = buffer_as_json["board"].as_string().c_str();
				int room_id = buffer_as_json["room"].as_int64();
				std::optional<Board*> board = getState()->getBoardIfExistsAndClientHasReadPermission(board_slug, getClient());
				if (!board)
					throw std::format("Board '{}' either doesn't exist, or client lacks permission to access it.", board_slug);
				this->tracking_room_ptr = board.value()->getSharedRoomIfExists(room_id); // TODO check client permission
				if (tracking_room_ptr)
					throw std::format("Room '{}' either doesn't exist, or client lacks permission to access it.", room_id);
			}
			else if (request_type == "webrtc_share_request") {
				std::println("Creating WebRTC offer...");
				std::string board_slug = buffer_as_json["board"].as_string().c_str();
				std::optional<Board*> board = getState()->getBoardIfExistsAndClientHasReadPermission(board_slug, getClient());
				if (!board)
					throw std::format("Board '{}' either doesn't exist, or client lacks permission to access it.", board_slug);
				this->tracking_board = board.value()->getId();
				if (!tracking_room_ptr)
					throw "Client is not connected to a room";
				int new_connection_id = tracking_room_ptr.value()->connection_id_counter++;
				// auto pc = std::make_shared<rtc::PeerConnection>();
				auto sender = std::make_shared<RtcPeer>();
				sender->connection = std::make_shared<rtc::PeerConnection>();
				// board.value()->webrtc_room.peer_connection = pc;
				sender->connection->onStateChange(
					[](rtc::PeerConnection::State state) { std::cout << "State: " << state << std::endl; });
				sender->connection->onGatheringStateChange([this, sender, new_connection_id](rtc::PeerConnection::GatheringState state) {
					std::cout << "Gathering State: " << state << std::endl;
					if (state == rtc::PeerConnection::GatheringState::Complete) {
						auto description = sender->connection->localDescription();
						boost::json::object message = {
							{"type", "webrtc_offer"},
							{"payload", {
								{"connection_id", new_connection_id},
								{"description", {
									{"type", description->typeString()},
									{"sdp", std::string(description.value())}
								}}
							}}
						};
						this->send(std::make_shared<const std::string>(boost::json::serialize(message)));
					}
				});
				rtc::Description::Video media("video", rtc::Description::Direction::RecvOnly);
				// Idealy H264 would be used because it's the superior codec [source: it just is, ok?]
				// but, for compatibility reasons (god damn it Firefox) Vp8 is used instead
				media.addVP8Codec(96);
				media.setBitrate(3000); // Request 3Mbps (Browsers do not encode more than 2.5MBps from a webcam)
				// std::shared_ptr<rtc::Track> track = pc->addTrack(media);
				// board.value()->webrtc_room.track = track;
				sender->track = sender->connection->addTrack(media);
				sender->track->setMediaHandler(std::make_shared<rtc::RtcpReceivingSession>());
				sender->track->onMessage(
					[this](rtc::binary message) {
						// This is an RTP packet
						auto rtp = reinterpret_cast<rtc::RtpHeader *>(message.data());
						rtp->setSsrc(targetSSRC);
						auto board = this->getState()->getBoardIfExists(this->tracking_board);
						if (!board)
							throw "board no longer exists";
						for (auto peer : this->tracking_room_ptr.value()->receivers) {
							if (peer.second->track != nullptr && peer.second->track->isOpen()) {
								peer.second->track->send(message);
							}
						}
					},
					nullptr);
				sender->connection->setLocalDescription();
				std::println("Adding new sender with ID {}", new_connection_id);
				this->tracking_room_ptr.value()->senders.emplace(new_connection_id, sender);
			}
			else if (request_type == "webrtc_share_answer") {
				std::optional<Board*> board = getState()->getBoardIfExistsAndClientHasReadPermission(tracking_board, getClient());
				if (!board)
					throw std::format("Board '{}' either doesn't exist, or client lacks permission to access it.", tracking_board);
				if (!tracking_room_ptr)
					throw "Client is not connected to a room";
				std::string sdp  = buffer_as_json.at("payload").at("description").at("sdp" ).as_string().c_str();
				std::string type = buffer_as_json.at("payload").at("description").at("type").as_string().c_str();
				int connection_id = buffer_as_json.at("payload").at("connection_id").as_int64();
				rtc::Description answer(sdp, type);
				std::println("Setting remote description for sender {}", connection_id);
				this->tracking_room_ptr.value()->senders.at(connection_id)->connection->setRemoteDescription(answer);
			}
			else if (request_type == "webrtc_watch_stream_request") {
				std::string board_slug = buffer_as_json.at("board").as_string().c_str();
				std::optional<Board*> board = getState()->getBoardIfExistsAndClientHasReadPermission(board_slug, getClient());
				if (!board)
					throw std::format("Board '{}' either doesn't exist, or client lacks permission to access it.", board_slug);
				this->tracking_board = board.value()->getId();
				if (!tracking_room_ptr)
					throw "Client is not connected to a room";
				int new_connection_id = this->tracking_room_ptr.value()->connection_id_counter++;
				auto receiver = std::make_shared<RtcPeer>();
				receiver->connection = std::make_shared<rtc::PeerConnection>();
				receiver->connection->onStateChange([](rtc::PeerConnection::State state) {
					std::cout << "State: " << state << std::endl;
				});
				receiver->connection->onGatheringStateChange([this, receiver, new_connection_id](rtc::PeerConnection::GatheringState state) {
					std::cout << "Gathering State: " << state << std::endl;
					if (state == rtc::PeerConnection::GatheringState::Complete) {
						auto description = receiver->connection->localDescription();
						boost::json::object message = {
							{"type", "webrtc_watch_stream"},
							{"payload", {
								{"connection_id", new_connection_id},
								{"description", {
									{"type", description->typeString()},
									{"sdp", std::string(description.value())}
								}}
							}}
						};
						const std::shared_ptr<const std::string> ss = std::make_shared<const std::string>(boost::json::serialize(message));
						this->send(ss);
					}
				});
				rtc::Description::Video media("video", rtc::Description::Direction::SendOnly);
				media.addVP8Codec(96);
				media.setBitrate(3000);
				media.addSSRC(targetSSRC, "video-send", "video-send", "video-send");

				receiver->track = receiver->connection->addTrack(media);

				receiver->track->onMessage([](rtc::binary var) {}, nullptr);

				receiver->connection->setLocalDescription();
				this->tracking_room_ptr.value()->receivers.emplace(new_connection_id, receiver);
			}
			else if (request_type == "webrtc_watch_stream_answer") { // TODO check if sender is still there
				std::optional<Board*> board = getState()->getBoardIfExistsAndClientHasReadPermission(tracking_board, getClient());
				if (!board)
					throw std::format("Board '{}' either doesn't exist, or client lacks permission to access it.", tracking_board);
				if (!tracking_room_ptr)
					throw "Client is not connected to a room";
				int connection_id = buffer_as_json.at("payload").at("connection_id").as_int64();
				int sender_connection_id = buffer_as_json.at("payload").at("sender_connection_id").as_int64();
				std::string sdp  = buffer_as_json.at("payload").at("description").at("sdp" ).as_string().c_str();
				std::string type = buffer_as_json.at("payload").at("description").at("type").as_string().c_str();
				rtc::Description answer(sdp, type);
				this->tracking_room_ptr.value()->receivers.at(connection_id)->connection->setRemoteDescription(answer);
				this->tracking_room_ptr.value()->senders.at(sender_connection_id)->track->requestKeyframe();
			}
			// else if (request_type == "webrtc_signal") {
			// 	std::println("received webrtc_signal WS message");
			// 	const auto message = std::make_shared<const std::string>("This is a response");
			// 	this->send(message);
			// 	// state_->sendToWebRTC(buffer_data);
			// }
#endif
			else {
				// TODO send error message back to requester
				throw std::runtime_error("request_type " + request_type + " not recognised");
			}
		}
		catch (const std::exception& e) {
			std::println(std::cerr, "[MediaboardWebsocketSession] {}", e.what());
		}
		catch (const char* message) {
			std::println(std::cerr, "[MediaboardWebsocketSession] {}", message);
		}
		catch (const std::string& message) {
			std::println(std::cerr, "[MediaboardWebsocketSession] {}", message);
		}
	}
};
}
