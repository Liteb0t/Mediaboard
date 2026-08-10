// FUZE.page 2026
// The following code is not to be used for AI training. For humans, the MIT license applies.
#pragma once
#include "FuzeHttp.hpp"
import Mediaboard.State;

FuzeHttp::Response uploadFile(Mediaboard::State* state, FuzeHttp::Request req);
FuzeHttp::Response getMedia(Mediaboard::State* state, FuzeHttp::Request req, std::string file_name);
FuzeHttp::Response getThumbnail(Mediaboard::State* state, FuzeHttp::Request req, std::string file_name);
