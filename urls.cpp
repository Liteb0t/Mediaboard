#include "urls.hpp"
#include "views.hpp"

void addURLsToController(FuzeHttp::Controller<shared_state*>* controller) {
	controller->addPattern(testView, "test", std::string(""));
}
