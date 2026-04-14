#include "views.hpp"
#include <boost/beast/http/status.hpp>
#include <iostream>

FuzeHttp::Response testView(shared_state* state, const http::request<http::string_body, http::basic_fields<std::allocator<char>>>& req, std::string var) {
	std::cout << "Called testview with var = " << var << std::endl;
	FuzeHttp::Response res;
	res.status = http::status::i_am_a_teapot;
	boost::json::object json;
	json["test"] = 73;
	json["req.target()"] = req.target();
	res.json = std::move(json);
	return res;
}
