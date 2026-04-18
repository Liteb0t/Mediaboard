#include "urls.hpp"
#include "views.hpp"

void addURLsToController(FuzeHttp::Controller<shared_state*>* controller) {
	// First arg takes the function name from views.cpp. The remaining args are URL sections.
	// C-style strings are immutable, and strings/ints are variables passed into the view.
	controller->addPattern(testView, "test", std::string(""));
	controller->addPattern(requestNewAccountParameters, "registration", "request_new_account_parameters");
	controller->addPattern(createNewAccount, "registration", "create_new_account");
}
