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
	std::filesystem::path servers_folder = state->root_path / "servers";
	if (!std::filesystem::exists(servers_folder)) {
		return {.status=http::status::bad_request, .error_message=std::format("servers_folder \"{}\" not found", servers_folder.string())};
	}
	std::filesystem::path new_server_folder = servers_folder / new_server_name;
	if (std::filesystem::exists(new_server_folder)) {
		return {.status=http::status::bad_request, .error_message=std::format("new_server_folder \"{}\" already exists", new_server_folder.string())};
	}
	std::filesystem::create_directory(new_server_folder);
	// TODO modify config.ini to set a different site_name and port
	std::filesystem::copy(state->mediaboard_data_path / "config.ini", new_server_folder);
	return Response{.status=http::status::ok};
}

FuzeHttp::Response showDocument(State* state, FuzeHttp::Request req) {
	// TODO handle target decoding in FuzeHttp
	std::string target = std::string(FuzeHttp::getPathName(FuzeHttp::getDecodedURL(req.target())).substr(1));
	return {
		.status = http::status::ok,
		.file = state->getDocumentRoot() / target
	};
}
} // namespace Mediaboard::Manager
