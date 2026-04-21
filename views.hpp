#pragma once
#include "shared_state.hpp"
#include "FuzeHttp.hpp"

FuzeHttp::Response testView(shared_state* state, const http::request<http::string_body, http::basic_fields<std::allocator<char>>>& req, std::string var);
FuzeHttp::Response requestNewAccountParameters(shared_state* state, FuzeHttp::Request req);
FuzeHttp::Response createNewAccount(shared_state* state, FuzeHttp::Request req);
FuzeHttp::Response requestLoginParameters(shared_state* state, FuzeHttp::Request req);
FuzeHttp::Response login(shared_state* state, FuzeHttp::Request req);
