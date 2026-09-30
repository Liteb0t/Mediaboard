// Copyright (c) 2026, Fuze.page
// Fuze Human-oriented License v1
module;
#include <boost/beast/http.hpp>
export module Mediaboard.URLs:media;
// #include "views.hpp"
// #include "views_media.hpp"
// #include "views_registration.hpp"
import FuzeHttp.Controller;
import Mediaboard.Permission;
import Mediaboard.Resolvers;
import Mediaboard.State;
import Mediaboard.Views_media;

using namespace FuzeHttp;
using namespace boost::beast::http;

export namespace Mediaboard {
void addMediaURLsToController(FuzeHttp::Controller<State*>* controller) {
	controller->addPatterns()
	(verb::post, uploadFileInThread,				"api",  "board", BoardResolver{}, "thread", ThreadResolver{}, "upload")
	(verb::post, uploadFileInBoard,					"api",  "board", BoardResolver{}, "upload")
	(verb::get, getMedia,							"media", std::string())
	(verb::get, getThumbnail,						"media", "thumbnails", std::string());
}

} // export namespace Mediaboard
