#include "views.hpp"
#include <boost/beast/http/status.hpp>
#include <iostream>

FuzeHttp::Response testView(shared_state* state, FuzeHttp::Request req, std::string var) {
	FuzeHttp::Response res;
	std::cout << "Called testview with var = " << var << std::endl;
	res.status = http::status::i_am_a_teapot;
	boost::json::object json;
	json["test"] = 73;
	json["req.target()"] = req.target();
	res.json = std::move(json);
	return res;
}

FuzeHttp::Response registerAccount(shared_state* state, FuzeHttp::Request req) {
	boost::json::value req_json;
	boost::json::string username;
	try {
		req_json = boost::json::parse(req.body());
		username = req_json.at("username").as_string();
	}
	catch(const std::exception& e) {
		return FuzeHttp::Response{.status = http::status::internal_server_error, .error_message = std::format("[registerAccount] {}", e.what())};
	}
	return FuzeHttp::Response{.status = http::status::ok};
}
