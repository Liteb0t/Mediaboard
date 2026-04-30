#pragma once
#include "shared_state.hpp"
#include "FuzeHttp.hpp"

FuzeHttp::Response requestNewAccountParameters(shared_state* state, FuzeHttp::Request req);
FuzeHttp::Response createNewAccount(shared_state* state, FuzeHttp::Request req);
FuzeHttp::Response requestLoginParameters(shared_state* state, FuzeHttp::Request req);
FuzeHttp::Response login(shared_state* state, FuzeHttp::Request req);
