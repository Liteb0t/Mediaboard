//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/vinniefalco/CppCon2018
//

#include "listener.hpp"
#include "http_session.hpp"
#include <iostream>

listener::listener(boost::asio::io_context& io_context, boost::asio::ip::tcp::endpoint endpoint, boost::shared_ptr<shared_state> const& state)
		: io_context_(io_context) , acceptor_(io_context) , state_(state) {
	beast::error_code ec;

	// Open the acceptor
	acceptor_.open(endpoint.protocol(), ec);
	if (ec) {
		fail(ec, "open");
		return;
	}

	// Allow address reuse
	acceptor_.set_option(boost::asio::socket_base::reuse_address(true), ec);
	if (ec) {
		fail(ec, "set_option");
		return;
	}

	// Bind to the server address
	acceptor_.bind(endpoint, ec);
	if (ec) {
		fail(ec, "bind");
		return;
	}

	// Start listening for connections
	acceptor_.listen(boost::asio::socket_base::max_listen_connections, ec);

	if (ec) {
		fail(ec, "listen");
		return;
	}
}

void listener::run() {
	// The new connection gets its own strand
	acceptor_.async_accept(
		boost::asio::make_strand(io_context_),
		beast::bind_front_handler(
			&listener::on_accept,
			shared_from_this()
		)
	);
}

// Report a failure
void listener::fail(beast::error_code ec, char const* what) {
	// Don't report on canceled operations
	if (ec == boost::asio::error::operation_aborted)
		return;
	std::cerr << what << ": " << ec.message() << "\n";
}

// Handle a connection
void listener::on_accept(beast::error_code ec, boost::asio::ip::tcp::socket socket) {
	if (ec)
		return fail(ec, "accept");
	else {
		// Launch a new session for this connection
		boost::make_shared<http_session>(
			std::move(socket),
			state_)->run();
	}

	// The new connection gets its own strand
	acceptor_.async_accept(
		boost::asio::make_strand(io_context_),
		beast::bind_front_handler(
			&listener::on_accept,
			shared_from_this()
		)
	);
}
