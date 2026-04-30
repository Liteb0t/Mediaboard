#pragma once
#include "shared_state.hpp"
#include "FuzeHttp.hpp"

FuzeHttp::Response threads(shared_state* state, FuzeHttp::Request req);
FuzeHttp::Response client(shared_state* state, FuzeHttp::Request req);
FuzeHttp::Response acceptInvite(shared_state* state, FuzeHttp::Request req, std::string invite_key_base64);
FuzeHttp::Response createThread(shared_state* state, FuzeHttp::Request req, FuzeHttp::Client client);
