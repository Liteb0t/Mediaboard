#pragma once
#include "shared_state.hpp"
#include "FuzeHttpServer.hpp"

namespace Mediaboard {
class WebsocketSession : public FuzeHttp::WebsocketSession {
public:
	WebsocketSession(boost::asio::ip::tcp::socket&& socket, FuzeHttp::State* state) : FuzeHttp::WebsocketSession(std::move(socket), state) {}
private:
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
			// else if (request_type == "connect_to_channel") {
			// 	std::println("DUMMY added ws to channel");
			// 	is_webrtc = true;
			// }
			// else if (request_type == "webrtc_signal") {
			// 	std::println("received webrtc_signal WS message");
			// 	state_->sendToWebRTC(buffer_data);
			// }
			else {
				// TODO send error message back to requester
				throw std::runtime_error("request_type " + request_type + " not recognised");
			}
		}
		catch (const std::exception& e) {
			std::println(std::cerr, "[WebsocketSession] {}", e.what());
		}
	}
	int tracking_thread;
};
}
