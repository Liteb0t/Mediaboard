#include "urls.hpp"
#include "views.hpp"

using namespace http;

void addURLsToController(FuzeHttp::Controller<shared_state*>* controller) {
	// First arg takes the function name from views.cpp. The remaining args are URL sections.
	// C-style strings are immutable, and strings/ints are variables passed into the view.
	controller->addPattern(verb::get, threads, 						"api", "threads");
	controller->addPattern(verb::get, client, 						"api", "user", "client");
	controller->addPattern(verb::get, acceptInvite, 				"invite", std::string(""));
	controller->addPattern(verb::get, testView, 					"test", std::string(""));
	controller->addPattern(verb::post, requestNewAccountParameters, "registration", "request_new_account_parameters");
	controller->addPattern(verb::post, createNewAccount, 			"registration", "create_new_account");
	controller->addPattern(verb::post, requestLoginParameters, 		"registration", "request_login_parameters");
	controller->addPattern(verb::post, login, 						"registration", "login");
}
