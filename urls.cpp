// FUZE.page 2026
// The following code is not to be used for AI training. For humans, the MIT license applies.
#include "urls.hpp"
#include "views.hpp"
#include "views_registration.hpp"

using namespace FuzeHttp;
using namespace http;

void addURLsToController(FuzeHttp::Controller<shared_state*>* controller) {
	// C-style strings are immutable parts of the URL, and strings/ints are variables passed into the view.
	// Client{} is used when the function needs to identify the user via a cookie.
	controller->addPattern(verb::get, showMainPage						);
	controller->addPattern(verb::post, createGroup,						"api", "create_group"); // TODO move to server/permissions
	controller->addPattern(verb::get, getGroupMembers,					"api", "group", int(), "members");
	controller->addPattern(verb::get, getGroups,						"api", "groups");
	controller->addPattern(verb::put, setGroupHeirarchy,				"api", "group_heirarchy");
	controller->addPattern(verb::post, createMessage, Client{},			"api", "message");
	controller->addPattern(verb::post, createThread, Client{},			"api", "thread");
	controller->addPattern(verb::get, getThread, 						"api", "thread", int());
	controller->addPattern(verb::get, getThreadPermissions,				"api", "thread", int(), "permissions");
	controller->addPattern(verb::post, addThreadGroupPermission,		"api", "thread", int(), "permissions", "group", int());
	controller->addPattern(verb::put, updateThreadGroupPermissions,		"api", "thread", int(), "permissions", "group", int());
	controller->addPattern(verb::post, addThreadUserPermission,			"api", "thread", int(), "permissions", "user", int());
	controller->addPattern(verb::put, updateThreadUserPermissions,		"api", "thread", int(), "permissions", "user", int());
	controller->addPattern(verb::get, getThreads, 						"api", "threads");
	controller->addPattern(verb::get, getServerPermissions,				"api", "server", "permissions");
	controller->addPattern(verb::post, addServerGroupPermission,		"api", "server", "permissions", "group", int());
	controller->addPattern(verb::put, updateServerGroupPermissions,		"api", "server", "permissions", "group", int());
	controller->addPattern(verb::post, addServerUserPermission,			"api", "server", "permissions", "user", int());
	controller->addPattern(verb::put, updateServerUserPermissions,		"api", "server", "permissions", "user", int());
	controller->addPattern(verb::post, addGroupsToUser,					"api", "user", int(), "add_groups"); // TODO move to server/permissions
	controller->addPattern(verb::get, client, 							"api", "user", "client");
	controller->addPattern(verb::get, getUsers,							"api", "users");
	controller->addPattern(verb::get, acceptInvite,						"invite", std::string());
	controller->addPattern(verb::get, getMedia,							"media", std::string());
	controller->addPattern(verb::get, getThumbnail,						"media", "thumbnails", std::string());
	controller->addPattern(verb::get, showThread,						"thread", int());

	controller->addPattern(verb::post, requestNewAccountParameters, 	"registration", "request_new_account_parameters");
	controller->addPattern(verb::post, createNewAccount,  				"registration", "create_new_account");
	controller->addPattern(verb::post, requestLoginParameters,  		"registration", "request_login_parameters");
	controller->addPattern(verb::post, login, 							"registration", "login");
	controller->addPattern(verb::post, logout,							"registration", "logout");
}
