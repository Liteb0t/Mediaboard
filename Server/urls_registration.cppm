// Copyright (c) 2026, Fuze.page
// Fuze Human-oriented License v1
module;
#include <boost/beast/http.hpp>
export module Mediaboard.URLs:registration;
// #include "views.hpp"
// #include "views_media.hpp"
// #include "views_registration.hpp"
import FuzeHttp.Controller;
import Mediaboard.Permission;
import Mediaboard.Resolvers;
import Mediaboard.State;
import Mediaboard.Views_registration;

using namespace FuzeHttp;
using namespace boost::beast::http;

export namespace Mediaboard {
void addRegistrationURLsToController(FuzeHttp::Controller<State*>* controller) {
	controller->addPatterns()
	(verb::post, requestNewAccountParameters, 		"registration", "request_new_account_parameters")
	(verb::post, createNewAccount,  				"registration", "create_new_account")
	(verb::post, requestLoginParameters,  			"registration", "request_login_parameters")
	(verb::post, login, 							"registration", "login")
	(verb::post, logout,							"registration", "logout")
	(verb::post, changePassword,					"registration", "change_password")
	;
}

} // export namespace Mediaboard
