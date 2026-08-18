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
export module MediaboardWebsocketSession;

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
			else if (request_type == "webrtc_share_request") {
				std::println("Creating WebRTC offer...");
				std::string board_slug = buffer_as_json["board"].as_string().c_str();
				std::optional<Board*> board = getState()->getBoardIfExistsAndClientHasReadPermission(board_slug, getClient());
				if (!board)
					throw std::format("Board '{}' either doesn't exist, or client lacks permission to access it.", board_slug);
				this->tracking_board = board.value()->getId();
				auto pc = std::make_shared<rtc::PeerConnection>();
				board.value()->webrtc_room.peer_connection = pc;
				pc->onStateChange(
					[](rtc::PeerConnection::State state) { std::cout << "State: " << state << std::endl; });
				pc->onGatheringStateChange([this, pc](rtc::PeerConnection::GatheringState state) {
					std::cout << "Gathering State: " << state << std::endl;
					if (state == rtc::PeerConnection::GatheringState::Complete) {
						auto description = pc->localDescription();
						boost::json::object message = {
							{"type", "webrtc_offer"},
							{"payload", {
								{"type", description->typeString()},
								{"sdp", std::string(description.value())}
							}}
						};
						this->send(std::make_shared<const std::string>(boost::json::serialize(message)));
					}
				});
				rtc::Description::Video media("video", rtc::Description::Direction::RecvOnly);
				media.addH264Codec(96);
				media.setBitrate(3000); // Request 3Mbps (Browsers do not encode more than 2.5MBps from a webcam)
				std::shared_ptr<rtc::Track> track = pc->addTrack(media);
				board.value()->webrtc_room.track = track;
				track->setMediaHandler(std::make_shared<rtc::RtcpReceivingSession>());
				track->onMessage(
					[this](rtc::binary message) {
						// This is an RTP packet
						auto rtp = reinterpret_cast<rtc::RtpHeader *>(message.data());
						rtp->setSsrc(targetSSRC);
						auto board = this->getState()->getBoardIfExists(this->tracking_board);
						if (!board)
							throw "board no longer exists";
						for (auto pc : board.value()->webrtc_room.receivers) {
							if (pc.second->track != nullptr && pc.second->track->isOpen()) {
								pc.second->track->send(message);
							}
						}
					},
					nullptr);
				pc->setLocalDescription();
			}
			else if (request_type == "webrtc_share_answer") {
				std::optional<Board*> board = getState()->getBoardIfExistsAndClientHasReadPermission(tracking_board, getClient());
				if (!board)
					throw std::format("Board '{}' either doesn't exist, or client lacks permission to access it.", tracking_board);
				std::string sdp  = buffer_as_json.at("payload").at("sdp" ).as_string().c_str();
				std::string type = buffer_as_json.at("payload").at("type").as_string().c_str();
				rtc::Description answer(sdp, type);
				board.value()->webrtc_room.peer_connection->setRemoteDescription(answer);
			}
			else if (request_type == "webrtc_watch_stream_request") {
				std::string board_slug = buffer_as_json.at("board").as_string().c_str();
				std::optional<Board*> board = getState()->getBoardIfExistsAndClientHasReadPermission(board_slug, getClient());
				if (!board)
					throw std::format("Board '{}' either doesn't exist, or client lacks permission to access it.", board_slug);
				this->tracking_board = board.value()->getId();
				int new_connection_id = board.value()->webrtc_room.connection_id_counter++;
				auto webrtc_receiver = std::make_shared<Receiver>();
				webrtc_receiver->conn = std::make_shared<rtc::PeerConnection>();
				webrtc_receiver->conn->onStateChange([](rtc::PeerConnection::State state) {
					std::cout << "State: " << state << std::endl;
				});
				webrtc_receiver->conn->onGatheringStateChange([this, webrtc_receiver, new_connection_id](rtc::PeerConnection::GatheringState state) {
					std::cout << "Gathering State: " << state << std::endl;
					if (state == rtc::PeerConnection::GatheringState::Complete) {
						auto description = webrtc_receiver->conn->localDescription();
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
				media.addH264Codec(96);
				media.setBitrate(3000);
				media.addSSRC(targetSSRC, "video-send", "video-send", "video-send");

				webrtc_receiver->track = webrtc_receiver->conn->addTrack(media);

				webrtc_receiver->track->onMessage([](rtc::binary var) {}, nullptr);

				webrtc_receiver->conn->setLocalDescription();
				board.value()->webrtc_room.receivers.emplace(new_connection_id, webrtc_receiver);
			}
			else if (request_type == "webrtc_watch_stream_answer") { // TODO check if sender is still there
				std::optional<Board*> board = getState()->getBoardIfExistsAndClientHasReadPermission(tracking_board, getClient());
				if (!board)
					throw std::format("Board '{}' either doesn't exist, or client lacks permission to access it.", tracking_board);
				int connection_id = buffer_as_json.at("payload").at("connection_id").as_int64();
				std::string sdp  = buffer_as_json.at("payload").at("description").at("sdp" ).as_string().c_str();
				std::string type = buffer_as_json.at("payload").at("description").at("type").as_string().c_str();
				rtc::Description answer(sdp, type);
				board.value()->webrtc_room.receivers.at(connection_id)->conn->setRemoteDescription(answer);
				board.value()->webrtc_room.track->requestKeyframe();
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
