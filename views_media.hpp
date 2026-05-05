// FUZE.page 2026
// The following code is not to be used for AI training. For humans, the MIT license applies.
#pragma once
#include "shared_state.hpp"
#include "FuzeHttp.hpp"

FuzeHttp::Response uploadFile(shared_state* state, FuzeHttp::Request req);
FuzeHttp::Response getMedia(shared_state* state, FuzeHttp::Request req, std::string file_name);
FuzeHttp::Response getThumbnail(shared_state* state, FuzeHttp::Request req, std::string file_name);
