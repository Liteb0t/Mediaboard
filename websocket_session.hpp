#pragma once
//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/vinniefalco/CppCon2018
//

#ifndef BOOST_BEAST_EXAMPLE_WEBSOCKET_CHAT_MULTI_WEBSOCKET_SESSION_HPP
#define BOOST_BEAST_EXAMPLE_WEBSOCKET_CHAT_MULTI_WEBSOCKET_SESSION_HPP

#include "beast.hpp"
#include "shared_state.hpp"

#include <boost/asio.hpp>
// #include <boost/hash2/sha1.hpp>
#include <cstdlib>
#include <memory>
#include <print>
#include <string>
#include <vector>

// Forward declaration
// class shared_state;
// class shared_state : public PermissionManager;

/** Represents an active WebSocket connection to the server
*/
class websocket_session : public boost::enable_shared_from_this<websocket_session> {
public:
	websocket_session(boost::asio::ip::tcp::socket&& socket, shared_state* state);
	~websocket_session();

	template<class Body, class Allocator>
	void run(http::request<Body, http::basic_fields<Allocator>> req);

	// Send a message
	void send(boost::shared_ptr<std::string const> const& ss);

	bool is_webrtc = false; // TODO: replace with abstract classes

	std::optional<Client> getClient() const { return this->client; }
private:
	int tracking_thread;
	beast::flat_buffer buffer_;
	websocket::stream<beast::tcp_stream> ws_;
	shared_state* state_;
	std::optional<Client> client;
	std::vector<boost::shared_ptr<std::string const>> queue_;

	void fail(beast::error_code ec, char const* what);
	void on_accept(beast::error_code ec);
	void on_read(beast::error_code ec, std::size_t bytes_transferred);
	void on_write(beast::error_code ec, std::size_t bytes_transferred);
	void on_send(boost::shared_ptr<std::string const> const& ss);
};

template<class Body, class Allocator>
void websocket_session::run(http::request<Body, http::basic_fields<Allocator>> req) {
	// Set suggested timeout settings for the websocket
	ws_.set_option(websocket::stream_base::timeout::suggested(beast::role_type::server));

	// Set a decorator to change the Server of the handshake
	ws_.set_option(websocket::stream_base::decorator(
		[](websocket::response_type& res) {
			res.set(http::field::server,
				std::string(BOOST_BEAST_VERSION_STRING) +
					" websocket-chat-multi");
		}
	));
	this->client = this->state_->getClientIfExists(req);
	/*
	auto sec_websocket_key_header = req.find("Sec-WebSocket-Key");
	if (sec_websocket_key_header == req.end())
		throw std::runtime_error("Sec-WebSocket-Key header not found");
	std::string sec_websocket_key = sec_websocket_key_header->value();
	std::println("line start[]{}[]line end", sec_websocket_key);
	sec_websocket_key += "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
	std::println("[websocket_session::run] sec_websocket_key: {}", sec_websocket_key);
	boost::hash2::sha1_160 hash;
	hash.update(sec_websocket_key.c_str(), sec_websocket_key.length());
	// unsigned char hash_res[20];
	// for (int i = 0; i < 20; ++i)
	// 	hash_res[i] = hash.result()[i];
	// char key_bytes[41];
	// boost::hash2::to_chars(hash.result(), key_bytes);
	char websocket_accept_base64[sodium_base64_ENCODED_LEN(20, sodium_base64_VARIANT_ORIGINAL)];
	sodium_bin2base64(
		websocket_accept_base64, sizeof websocket_accept_base64,
		hash.result().data(), 20,
		// (unsigned char*)key_bytes, 20,
		sodium_base64_VARIANT_ORIGINAL
	);
	std::println("[websocket_session::run] key_base64: {}", websocket_accept_base64);

	auto basic_res = FuzeHttp::Response{
		.status = http::status::switching_protocols,
		.headers = {{
			{"Sec-Websocket-Accept", websocket_accept_base64}
		}}
	};
	http::message_generator msg = FuzeHttp::buildResponse<http::empty_body>(basic_res, req);
	*/

	// Accept the websocket handshake
	ws_.async_accept(
		req,
		beast::bind_front_handler(
			&websocket_session::on_accept,
			shared_from_this()
		)
	);
}

#endif
