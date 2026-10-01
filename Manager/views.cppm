// Copyright (c) 2026, Fuze.page
// Fuze Human-oriented License v1
module;
// #include "views.hpp"
#include <boost/beast/http/status.hpp>
#include <boost/json.hpp>
#include <filesystem>
#include <iostream>
#include <print>
#include "Request.hpp"
#include <unordered_set>
export module Mediaboard.Manager.Views;

import FuzeHttp.Core;
import FuzeHttp.PermissionObject;
import FuzeHttp.Utils;
import Mediaboard.Manager.State;

using namespace FuzeHttp;
export namespace Mediaboard::Manager {

FuzeHttp::Response createServer(State* state, Request req, std::string new_server_name) {
	if (auto res = state->createServer(new_server_name))
		return Response{.status=http::status::ok};
	else
		return res.error();
}

FuzeHttp::Response showDocument(State* state, FuzeHttp::Request req) {
	// TODO handle target decoding in FuzeHttp
	std::string target = std::string(FuzeHttp::getPathName(FuzeHttp::getDecodedURL(req.target())).substr(1));
	return {
		.status = http::status::ok,
		.file = state->getDocumentRoot() / target
	};
}

FuzeHttp::Response updateServers(State* state, FuzeHttp::Request req) {
	if (auto res = state->updateServers(); res)
		return {.status = http::status::ok};
	else
		return {.status = http::status::internal_server_error, .error_message = res.error()};
}

Response listServers(State* state, Request req) {
	return {
		.status = http::status::ok,
		.json = {{{"mediaboard_servers", state->getServersAsJson()}}}
	};
}
} // namespace Mediaboard::Manager
