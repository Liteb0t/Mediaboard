// Copyright (c) 2026, Fuze.page
// Fuze Human-oriented License v1
module;
#include <boost/beast/http.hpp>
export module Mediaboard.Manager.URLs;

import FuzeHttp.Controller;
import Mediaboard.Manager.Resolvers;
import Mediaboard.Manager.State;
import Mediaboard.Manager.Views;

using namespace FuzeHttp;
using namespace boost::beast::http;

export namespace Mediaboard {
namespace Manager {
void addURLsToController(FuzeHttp::Controller<State*>* controller) {

	controller->addPatterns()
	// (verb::get, showMainPage,						"boards")
	;
}
}
} // export namespace Mediaboard
