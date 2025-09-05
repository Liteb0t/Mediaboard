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
#include <nlohmann/json.hpp>
using json = nlohmann::json;

websocket_session::
websocket_session(
    tcp::socket&& socket,
    boost::shared_ptr<shared_state> const& state)
    : ws_(std::move(socket))
    , state_(state)
{
	this->tracking_thread = -1;
}

websocket_session::
~websocket_session()
{
    // Remove this session from the list of active sessions
    state_->leave(this);
	state_->main_board.removeListenerFromThread(this, this->tracking_thread);
}

void
websocket_session::
fail(beast::error_code ec, char const* what)
{
    // Don't report these
    if( ec == net::error::operation_aborted ||
        ec == websocket::error::closed)
        return;

    std::cerr << what << ": " << ec.message() << "\n";
}

void
websocket_session::
on_accept(beast::error_code ec)
{
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
            shared_from_this()));
}

void
websocket_session::
on_read(beast::error_code ec, std::size_t)
{
    // Handle the error, if any
    if(ec)
        return fail(ec, "read");

	std::string buffer_data = beast::buffers_to_string(buffer_.data());
	std::cout << buffer_data << std::endl;
	json buffer_as_json = json::parse(buffer_data);

	std::string request_type = buffer_as_json["type"].template get<std::string>();
	if (request_type == "listen_to_thread") {
		if (buffer_as_json["thread_id"].is_number_integer()) {
			int thread_id = buffer_as_json["thread_id"].template get<int>();
			if (state_->main_board.threadExists(thread_id)) {
				this->tracking_thread = thread_id;
				state_->main_board.addListenerToThread(this, thread_id);
			}
			else
				std::cout << "Warning: thread " << thread_id << " does not exist" << std::endl;
		}
		else {
			std::cout << "Warning: thread is not an integer" << std::endl;
		}
	}
	// if (request_type == "create_thread") {
	// 	// Validate JSON
	// 	if (buffer_as_json["post_zero"]["files"].size() > 4) {
	// 		std::cerr << "Denied: More than 4 files in post\n";
	// 		return;
	// 	}
	// 	else {
	// 		// state_->main_board.createPost(buffer_as_json["post"]);
	// 		state_->main_board.createThread(buffer_as_json["thread"]);
	// 		// Send to all connections
	// 		// state_->send(state_->main_board.dumpLastThread());
	// 	}
	// }
	// else if (request_type == "create_post") {
	// 	// state_->send(state_->main_board.createPost(buffer_as_json["post"]));
	// 	state_->main_board.createPost(buffer_as_json["post"]);
	// }
	// else if (request_type == "fetch_catalog") {
	// 	state_->send(state_->main_board.dumpAllThreads());
	// }
	// else if (request_type == "fetch_thread_posts") {
	// 	state_->send(state_->main_board.dumpPostsInThread(buffer_as_json["thread_id"].template get<int>()));
	// }
	else {
		// TODO send error message back to requester
		std::cerr << "request_type " + request_type + " not recognised" << std::endl;
	}

    // Clear the buffer
    buffer_.consume(buffer_.size());

    // Read another message
    ws_.async_read(
        buffer_,
        beast::bind_front_handler(
            &websocket_session::on_read,
            shared_from_this()));
}

void
websocket_session::
send(boost::shared_ptr<std::string const> const& ss)
{
    // Post our work to the strand, this ensures
    // that the members of `this` will not be
    // accessed concurrently.

    net::post(
        ws_.get_executor(),
        beast::bind_front_handler(
            &websocket_session::on_send,
            shared_from_this(),
            ss));
}

void
websocket_session::
on_send(boost::shared_ptr<std::string const> const& ss)
{
    // Always add to queue
    queue_.push_back(ss);

    // Are we already writing?
    if(queue_.size() > 1)
        return;

    // We are not currently writing, so send this immediately
    ws_.async_write(
        net::buffer(*queue_.front()),
        beast::bind_front_handler(
            &websocket_session::on_write,
            shared_from_this()));
}

void
websocket_session::
on_write(beast::error_code ec, std::size_t)
{
    // Handle the error, if any
    if(ec)
        return fail(ec, "write");

    // Remove the string from the queue
    queue_.erase(queue_.begin());

    // Send the next message if any
    if(! queue_.empty())
        ws_.async_write(
            net::buffer(*queue_.front()),
            beast::bind_front_handler(
                &websocket_session::on_write,
                shared_from_this()));
}
