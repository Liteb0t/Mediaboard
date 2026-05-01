#pragma once
#include "shared_state.hpp"
#include "FuzeHttp.hpp"

FuzeHttp::Response showMainPage(shared_state* state, FuzeHttp::Request req);
FuzeHttp::Response createThread(shared_state* state, FuzeHttp::Request req, FuzeHttp::Client client);
FuzeHttp::Response showThread(shared_state* state, FuzeHttp::Request req, int thread_id);
FuzeHttp::Response getThreadPermissions(shared_state* state, FuzeHttp::Request req, int thread_id);
FuzeHttp::Response showThreads(shared_state* state, FuzeHttp::Request req);
FuzeHttp::Response client(shared_state* state, FuzeHttp::Request req);
FuzeHttp::Response acceptInvite(shared_state* state, FuzeHttp::Request req, std::string invite_key_base64);
