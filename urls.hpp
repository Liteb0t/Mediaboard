#pragma once
#include "FuzeHttp.hpp"
#include "shared_state.hpp"

void addURLsToController(FuzeHttp::Controller<shared_state*>* controller);
