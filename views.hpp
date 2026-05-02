#pragma once
#include "shared_state.hpp"
#include "FuzeHttp.hpp"

FuzeHttp::Response showMainPage(shared_state* state, FuzeHttp::Request req);
FuzeHttp::Response createThread(shared_state* state, FuzeHttp::Request req, FuzeHttp::Client client);
FuzeHttp::Response getThread(shared_state* state, FuzeHttp::Request req, int thread_id);
FuzeHttp::Response getThreadPermissions(shared_state* state, FuzeHttp::Request req, int thread_id);
FuzeHttp::Response getThreads(shared_state* state, FuzeHttp::Request req);
FuzeHttp::Response client(shared_state* state, FuzeHttp::Request req);
FuzeHttp::Response acceptInvite(shared_state* state, FuzeHttp::Request req, std::string invite_key_base64);
FuzeHttp::Response getMedia(shared_state* state, FuzeHttp::Request req, std::string file_name);
FuzeHttp::Response getThumbnail(shared_state* state, FuzeHttp::Request req, std::string file_name);
FuzeHttp::Response showThread(shared_state* state, FuzeHttp::Request req, int thread_id);
