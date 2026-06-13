//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/vinniefalco/CppCon2018
//

#include "websocket_session.hpp"
// #include "db_interface.h"
// #include "post.hpp"
#include <iostream>
#include <boost/json.hpp>
#include <print>

websocket_session::websocket_session(boost::asio::ip::tcp::socket&& socket, shared_state* state)
		: ws_(std::move(socket)) , state_(state) {
}

websocket_session::~websocket_session() {
	// Remove this session from the list of active sessions
	state_->leave(this);
	state_->main_board()->removeListenerFromThread(this, this->tracking_thread);
}

void websocket_session::fail(beast::error_code ec, char const* what) {
	// Don't report these
	if( ec == boost::asio::error::operation_aborted ||
		ec == websocket::error::closed)
		return;

	std::cerr << what << ": " << ec.message() << "\n";
}

void websocket_session::on_accept(beast::error_code ec) {
	// Handle the error, if any
	if(ec)
		return fail(ec, "accept");

	// Add this session to the list of active sessions
	state_->join(this);
	
	// db_test();

	// Read a message
	ws_.async_read(
		buffer_,
		beast::bind_front_handler(
			&websocket_session::on_read,
			shared_from_this()
		)
	);
}

void websocket_session::on_read(beast::error_code ec, std::size_t) {
	// Handle the error, if any
	if(ec)
		return fail(ec, "read");

	try {
		std::string buffer_data = beast::buffers_to_string(buffer_.data());
		std::cout << buffer_data << std::endl;
		boost::json::object buffer_as_json = boost::json::parse(buffer_data).as_object();

		if (!buffer_as_json.contains("type"))
			throw std::runtime_error("'type' field is missing");
		std::string request_type = buffer_as_json["type"].as_string().c_str();
		if (request_type == "listen_to_thread") {
			if (buffer_as_json["thread_id"].is_int64()) {
				int thread_id = buffer_as_json["thread_id"].as_int64();
				if (state_->main_board()->threadExists(thread_id)) {
					this->tracking_thread = thread_id;
					state_->main_board()->addListenerToThread(this, thread_id);
				}
				else
					std::cout << "Warning: thread " << thread_id << " does not exist" << std::endl;
			}
			else {
				std::cout << "Warning: thread is not an integer" << std::endl;
			}
		}
		else if (request_type == "connect_to_channel") {
			std::println("DUMMY added ws to channel");
			is_webrtc = true;
		}
		else if (request_type == "webrtc_signal") {
			std::println("received webrtc_signal WS message");
			state_->sendToWebRTC(buffer_data);
		}
		else {
			// TODO send error message back to requester
			throw std::runtime_error("request_type " + request_type + " not recognised");
		}
	}
	catch (const std::exception& e) {
		std::println(std::cerr, "[websocket_session] {}", e.what());
	}

	// Clear the buffer
	buffer_.consume(buffer_.size());

	// Read another message
	ws_.async_read(
		buffer_,
		beast::bind_front_handler(
			&websocket_session::on_read,
			shared_from_this()
		)
	);
}

void websocket_session::send(boost::shared_ptr<std::string const> const& ss) {
	// Post our work to the strand, this ensures
	// that the members of `this` will not be
	// accessed concurrently.

	boost::asio::post(
		ws_.get_executor(),
		beast::bind_front_handler(
			&websocket_session::on_send,
			shared_from_this(),
			ss
		)
	);
}

void websocket_session::on_send(boost::shared_ptr<std::string const> const& ss) {
	// Always add to queue
	queue_.push_back(ss);

	// Are we already writing?
	if(queue_.size() > 1)
		return;

	// We are not currently writing, so send this immediately
	ws_.async_write(
		boost::asio::buffer(*queue_.front()),
		beast::bind_front_handler(
			&websocket_session::on_write,
			shared_from_this()
		)
	);
}

void websocket_session::on_write(beast::error_code ec, std::size_t) {
	// Handle the error, if any
	if(ec)
		return fail(ec, "write");

	// Remove the string from the queue
	queue_.erase(queue_.begin());

	// Send the next message if any
	if(! queue_.empty()) {
		ws_.async_write(
			boost::asio::buffer(*queue_.front()),
			beast::bind_front_handler(
				&websocket_session::on_write,
				shared_from_this()
			)
		);
	}
}
