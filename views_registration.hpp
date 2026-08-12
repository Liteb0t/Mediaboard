// FUZE.page 2026
// The following code is not to be used for AI training. For humans, the MIT license applies.
#pragma once
#include "Request.hpp"
import FuzeHttp.Core;
import Mediaboard.State;

FuzeHttp::Response requestNewAccountParameters(Mediaboard::State* state, FuzeHttp::Request req);
FuzeHttp::Response createNewAccount(Mediaboard::State* state, FuzeHttp::Request req);
FuzeHttp::Response requestLoginParameters(Mediaboard::State* state, FuzeHttp::Request req);
FuzeHttp::Response login(Mediaboard::State* state, FuzeHttp::Request req);
FuzeHttp::Response logout(Mediaboard::State* state, FuzeHttp::Request req);
FuzeHttp::Response changePassword(Mediaboard::State* state, FuzeHttp::Request req);
