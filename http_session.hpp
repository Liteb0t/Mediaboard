//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/vinniefalco/CppCon2018
//

#ifndef BOOST_BEAST_EXAMPLE_WEBSOCKET_CHAT_MULTI_HTTP_SESSION_HPP
#define BOOST_BEAST_EXAMPLE_WEBSOCKET_CHAT_MULTI_HTTP_SESSION_HPP

#include "beast.hpp"
// #include "shared_state.hpp"
#include "FuzeHttp.hpp"
#include "WebsocketSession.hpp"

#include <boost/asio.hpp>
#include <boost/optional.hpp>
#include <boost/smart_ptr.hpp>
#include <cstdlib>
#include <memory>

namespace FuzeHttp {
// Represents an established HTTP connection

// Return a response for the given request.
//
// The concrete type of the response message (which depends on the
// request), is type-erased in message_generator.
template<class StateType>
class http_session : public boost::enable_shared_from_this<http_session<StateType>> {
public:
	http_session(boost::asio::ip::tcp::socket&& socket, StateType* state, FuzeHttp::Controller<StateType*>* controller)
			: stream_(std::move(socket)),
			state_(state),
			controller(controller) {
	}

	void run() {
		do_read();
	}

private:
	FuzeHttp::Controller<StateType*>* controller;
	beast::tcp_stream stream_;
	beast::flat_buffer buffer_;
	StateType* state_;

	// The parser is stored in an optional container so we can
	// construct it from scratch it at the beginning of each new message.
	boost::optional<http::request_parser<http::string_body>> parser_;

	struct send_lambda;
	static http::message_generator handle_request(
		StateType* state,
		FuzeHttp::Controller<StateType*>* controller,
		http::request<http::string_body, http::basic_fields<std::allocator<char>>>&& req);

	void fail(beast::error_code ec, char const* what) {
		// Don't report on canceled operations
		if(ec == boost::asio::error::operation_aborted)
			return;

		std::cerr << what << ": " << ec.message() << "\n";
	}
	void do_read() {
		// Construct a new parser for each message
		parser_.emplace();

		// Apply a reasonable limit to the allowed size
		// of the body in bytes to prevent abuse.
		parser_->body_limit(this->state_->config.file_size_limit_mb << 20);
		// parser_->body_limit(25 << 20);

		// Set the timeout.
		stream_.expires_after(std::chrono::minutes(60));

		// Read a request
		http::async_read(
			stream_,
			buffer_,
			*parser_,
			beast::bind_front_handler(
				&http_session::on_read,
				this->shared_from_this()));
	}
	void on_read(beast::error_code ec, std::size_t) {
		// This means they closed the connection
		if(ec == http::error::end_of_stream) {
			stream_.socket().shutdown(boost::asio::ip::tcp::socket::shutdown_send, ec);
			return;
		}

		// Handle the error, if any
		if(ec)
			return fail(ec, "read");

		// See if it is a WebSocket Upgrade
		if(websocket::is_upgrade(parser_->get())) {
			// Create a websocket session, transferring ownership
			// of both the socket and the HTTP request.
			boost::make_shared<WebsocketSession>(stream_.release_socket(), state_)->run(parser_->release());
			return;
		}
		else {
			// Handle request
			http::message_generator msg = handle_request(state_, controller, parser_->release());
			// http::message_generator msg = handle_request(state_->doc_root(), parser_->release());

			// Determine if we should close the connection
			bool keep_alive = msg.keep_alive();

			auto self = this->shared_from_this();

			// Send the response
			beast::async_write(
				stream_, std::move(msg),
				[self, keep_alive](beast::error_code ec, std::size_t bytes) {
					self->on_write(ec, bytes, keep_alive);
				}
			);
		}
	}
	void on_write(beast::error_code ec, std::size_t, bool keep_alive) {
		// Handle the error, if any
		if(ec)
			return fail(ec, "write");

		if(!keep_alive) {
			// This means we should close the connection, usually because
			// the response indicated the "Connection: close" semantic.
			stream_.socket().shutdown(boost::asio::ip::tcp::socket::shutdown_send, ec);
			return;
		}

		// Read another request
		do_read();
	}
};

template<class StateType>
http::message_generator http_session<StateType>::handle_request(
		StateType* state,
		FuzeHttp::Controller<StateType*>* controller,
		http::request<http::string_body, http::basic_fields<std::allocator<char>>>&& req) {
	// Matches paths in urls.cpp
	FuzeHttp::Response basic_res;
	try {
		basic_res = controller->matchPathAndExecute(state, req);
		std::cout << "[http_session] basic_res.status: " << basic_res.status << std::endl;
		if (basic_res.json || basic_res.body)
			return FuzeHttp::buildResponse<http::string_body>(basic_res, req);
		else if (basic_res.file) {
			// if (!std::filesystem::is_regular_file(basic_res.file.value()))
			// 	basic_res.file = basic_res.file.value() / "index.html";
			std::println("Checking if file exists: {}", basic_res.file.value().string());
			if (!std::filesystem::exists(basic_res.file.value()))
				return FuzeHttp::buildResponse<http::empty_body>(FuzeHttp::Response{.status=http::status::bad_request, .error_message="File not found"}, req);
			else
				return FuzeHttp::buildResponse<http::file_body>(basic_res, req);
		}
		else
			return FuzeHttp::buildResponse<http::empty_body>(basic_res, req);
	}
	catch(const std::exception& e) {
		std::string error_text = std::format("[http_session] {}", e.what());
		std::cerr << error_text << std::endl;
		basic_res = FuzeHttp::Response{
			.status = http::status::internal_server_error,
			.error_message = error_text
		};
		return FuzeHttp::buildResponse<http::empty_body>(basic_res, req);
	}
}
} // namespace FuzeHttp
#endif
