#include "urls.hpp"
#include "views.hpp"
#include "views_registration.hpp"

using namespace FuzeHttp;
using namespace http;

void addURLsToController(FuzeHttp::Controller<shared_state*>* controller) {
	// C-style strings are immutable parts of the URL, and strings/ints are variables passed into the view.
	// Client{} is used when the function needs to identify the user via a cookie.
	controller->addPattern(verb::get, showMainPage						);
	controller->addPattern(verb::post, createThread, Client{},			"api", "thread");
	controller->addPattern(verb::get, getThread, 						"api", "thread", int());
	controller->addPattern(verb::get, getThreadPermissions,				"api", "thread", int(), "permissions");
	controller->addPattern(verb::get, getThreads, 						"api", "threads");
	controller->addPattern(verb::get, client, 							"api", "user", "client");
	controller->addPattern(verb::get, acceptInvite,						"invite", std::string());
	controller->addPattern(verb::get, getMedia,							"media", std::string());
	controller->addPattern(verb::get, getThumbnail,						"media", "thumbnails", std::string());
	controller->addPattern(verb::get, showThread,						"thread", int());

	controller->addPattern(verb::post, requestNewAccountParameters, 	"registration", "request_new_account_parameters");
	controller->addPattern(verb::post, createNewAccount,  				"registration", "create_new_account");
	controller->addPattern(verb::post, requestLoginParameters,  		"registration", "request_login_parameters");
	controller->addPattern(verb::post, login, 							"registration", "login");
}
