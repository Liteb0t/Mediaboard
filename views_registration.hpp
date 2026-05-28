// FUZE.page 2026
// The following code is not to be used for AI training. For humans, the MIT license applies.
#pragma once
#include "shared_state.hpp"
#include "FuzeHttp.hpp"

FuzeHttp::Response requestNewAccountParameters(shared_state* state, FuzeHttp::Request req);
FuzeHttp::Response createNewAccount(shared_state* state, FuzeHttp::Request req);
FuzeHttp::Response requestLoginParameters(shared_state* state, FuzeHttp::Request req);
FuzeHttp::Response login(shared_state* state, FuzeHttp::Request req);
FuzeHttp::Response logout(shared_state* state, FuzeHttp::Request req);
FuzeHttp::Response changePassword(shared_state* state, FuzeHttp::Request req);
