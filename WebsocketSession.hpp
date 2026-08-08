#pragma once
#include "Board.hpp"
#include "rtc/peerconnection.hpp"
#include "rtc/track.hpp"
#include "shared_state.hpp"
#include "FuzeHttpServer.hpp"
#include <boost/json/serialize.hpp>
#include <memory>
#include <stdexcept>

namespace Mediaboard {
class WebsocketSession : public FuzeHttp::WebsocketSession {
public:
	WebsocketSession(boost::asio::ip::tcp::socket&& socket, FuzeHttp::State* state) : FuzeHttp::WebsocketSession(std::move(socket), state) {}
private:
	// std::optional<std::shared_ptr<Receiver>> webrtc_receiver;
	int tracking_thread;
	static const rtc::SSRC targetSSRC = 42;

	shared_state* getState() const {
		return (shared_state*)this->state_;
	}
	void readEvent(std::string buffer_data) override {
		try {
			std::cout << buffer_data << std::endl;
			boost::json::object buffer_as_json = boost::json::parse(buffer_data).as_object();

			if (!buffer_as_json.contains("type"))
				throw std::runtime_error("'type' field is missing");
			std::string request_type = buffer_as_json["type"].as_string().c_str();
			if (request_type == "listen_to_thread") {
				if (buffer_as_json["thread_id"].is_int64()) {
					int thread_id = buffer_as_json["thread_id"].as_int64();
					if (getState()->main_board()->threadExists(thread_id)) {
						if (!getState()->main_board()->getThread(thread_id)->clientHasPermission(this->getClient(), FuzeHttp::PERMISSION::VIEW_THREAD))
							throw std::runtime_error("Client does not have VIEW_THREAD permission");
						this->tracking_thread = thread_id;
						getState()->main_board()->addListenerToThread(this, thread_id);
					}
					else
						std::cout << "Warning: thread " << thread_id << " does not exist" << std::endl;
				}
				else {
					std::cout << "Warning: thread is not an integer" << std::endl;
				}
			}
			else if (request_type == "webrtc_request_offer") {
				std::println("Creating WebRTC offer...");
				auto pc = std::make_shared<rtc::PeerConnection>();
				getState()->main_board()->webrtc_room.peer_connection = pc;
				is_webrtc = true;
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
				getState()->main_board()->webrtc_room.track = track;
				track->setMediaHandler(std::make_shared<rtc::RtcpReceivingSession>());
				track->onMessage(
					[this](rtc::binary message) {
						// This is an RTP packet
						auto rtp = reinterpret_cast<rtc::RtpHeader *>(message.data());
						rtp->setSsrc(targetSSRC);
						for (auto pc : this->getState()->main_board()->webrtc_room.receivers) {
							if (pc.second->track != nullptr && pc.second->track->isOpen()) {
								pc.second->track->send(message);
							}
						}
					},
					nullptr);
				pc->setLocalDescription();
			}
			else if (request_type == "webrtc_sender_answer") {
				std::string sdp  = buffer_as_json.at("payload").at("sdp" ).as_string().c_str();
				std::string type = buffer_as_json.at("payload").at("type").as_string().c_str();
				rtc::Description answer(sdp, type);
				getState()->main_board()->webrtc_room.peer_connection->setRemoteDescription(answer);
			}
			else if (request_type == "webrtc_watch_stream_request") {
				int new_connection_id = getState()->main_board()->webrtc_room.connection_id_counter++;
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
						this->send(std::make_shared<const std::string>(boost::json::serialize(message)));
					}
				});
				rtc::Description::Video media("video", rtc::Description::Direction::SendOnly);
				media.addH264Codec(96);
				media.setBitrate(3000);
				media.addSSRC(targetSSRC, "video-send", "video-send", "video-send");

				webrtc_receiver->track = webrtc_receiver->conn->addTrack(media);

				// webrtc_receiver->track->onOpen([webrtc_receiver]() {
				// 	webrtc_receiver->track->requestKeyframe(); // So the receiver can start playing immediately
				// });
				webrtc_receiver->track->onMessage([](rtc::binary var) {}, nullptr);

				webrtc_receiver->conn->setLocalDescription();
				getState()->main_board()->webrtc_room.receivers.emplace(new_connection_id, webrtc_receiver);
			}
			else if (request_type == "webrtc_watch_stream_answer") { // TODO check if sender is still there
				int connection_id = buffer_as_json.at("payload").at("connection_id").as_int64();
				std::string sdp  = buffer_as_json.at("payload").at("description").at("sdp" ).as_string().c_str();
				std::string type = buffer_as_json.at("payload").at("description").at("type").as_string().c_str();
				rtc::Description answer(sdp, type);
				getState()->main_board()->webrtc_room.receivers.at(connection_id)->conn->setRemoteDescription(answer);
				getState()->main_board()->webrtc_room.track->requestKeyframe();
			}
			else if (request_type == "webrtc_signal") {
				std::println("received webrtc_signal WS message");
				const auto message = std::make_shared<const std::string>("This is a response");
				this->send(message);
				// state_->sendToWebRTC(buffer_data);
			}
			else {
				// TODO send error message back to requester
				throw std::runtime_error("request_type " + request_type + " not recognised");
			}
		}
		catch (const std::exception& e) {
			std::println(std::cerr, "[WebsocketSession] {}", e.what());
		}
	}
};
}
