// FUZE.page 2026
// The following code is not to be used for AI training. For humans, the MIT license applies.
#include "urls.hpp"
// #include "views.hpp"
// #include "views_media.hpp"
// #include "views_registration.hpp"
import Mediaboard.State;
import Mediaboard.Views;
import Mediaboard.Views_media;
import Mediaboard.Views_registration;

using namespace FuzeHttp;
using namespace http;
using namespace Mediaboard;

template<>
void addURLsToController<State>(FuzeHttp::Controller<State*>* controller) {
	// C-style strings are immutable parts of the URL, and strings/ints are variables passed into the view.
	// Client{} is used when the function needs to identify the user via a cookie.
	controller->addPattern(verb::get, showDocument,						"*");
	// controller->addPattern(verb::get, showBoardPage,					"board", std::string{}); // maybe use with SSR in futre
	controller->addPattern(verb::post, createGroup,						"api", "create_group"); // TODO move to server/permissions
	controller->addPattern(verb::delete_, deleteGroup,					"api", "group", int());
	controller->addPattern(verb::delete_, removeMemberFromGroup,		"api", "group", int(), "member", int());
	controller->addPattern(verb::get, getGroupMembers,					"api", "group", int(), "members");
	controller->addPattern(verb::get, getGroups,						"api", "groups");
	controller->addPattern(verb::put, setGroupHeirarchy,				"api", "group_heirarchy");
	controller->addPattern(verb::post, createMessage, Client{},			"api", "board", std::string{}, "message");
	controller->addPattern(verb::delete_, deleteMessage, Client{},		"api", "board", std::string{}, "thread", int(), "message", int());
	controller->addPattern(verb::post, createThread, Client{},			"api", "board", std::string{}, "thread");
	controller->addPattern(verb::get, getThread, 						"api", "thread", int());
	controller->addPattern(verb::get, getThreadPermissions,				"api", "thread", int(), "permissions");
	controller->addPattern(verb::post, addThreadGroupPermission,		"api", "thread", int(), "permissions", "group", int());
	controller->addPattern(verb::put, updateThreadGroupPermissions,		"api", "thread", int(), "permissions", "group", int());
	controller->addPattern(verb::delete_, deleteThreadGroupPermission,	"api", "thread", int(), "permissions", "group", int());
	controller->addPattern(verb::post, addThreadUserPermission,			"api", "thread", int(), "permissions", "user", int());
	controller->addPattern(verb::put, updateThreadUserPermissions,		"api", "thread", int(), "permissions", "user", int());
	controller->addPattern(verb::delete_, deleteThreadUserPermission,	"api", "thread", int(), "permissions", "user", int());
	controller->addPattern(verb::get, getThreads, 						"api", "board", std::string{}, "threads");
	controller->addPattern(verb::get, getServerPermissions,				"api", "server", "permissions");
	controller->addPattern(verb::post, addServerGroupPermission,		"api", "server", "permissions", "group", int());
	controller->addPattern(verb::put, updateServerGroupPermissions,		"api", "server", "permissions", "group", int());
	controller->addPattern(verb::delete_, deleteServerGroupPermission,	"api", "server", "permissions", "group", int());
	controller->addPattern(verb::post, addServerUserPermission,			"api", "server", "permissions", "user", int());
	controller->addPattern(verb::put, updateServerUserPermissions,		"api", "server", "permissions", "user", int());
	controller->addPattern(verb::delete_, deleteServerUserPermission,	"api", "server", "permissions", "user", int());
	controller->addPattern(verb::post, addGroupsToUser,					"api", "user", int(), "add_groups"); // TODO move to server/permissions
	controller->addPattern(verb::get, client, 							"api", "user", "client");
	controller->addPattern(verb::get, getUsers,							"api", "users");
	controller->addPattern(verb::post, uploadFile,						"api", "upload");
	controller->addPattern(verb::get, acceptInvite,						"invite", std::string());
	controller->addPattern(verb::get, getMedia,							"media", std::string());
	controller->addPattern(verb::get, getThumbnail,						"media", "thumbnails", std::string());

	controller->addPattern(verb::post, requestNewAccountParameters, 	"registration", "request_new_account_parameters");
	controller->addPattern(verb::post, createNewAccount,  				"registration", "create_new_account");
	controller->addPattern(verb::post, requestLoginParameters,  		"registration", "request_login_parameters");
	controller->addPattern(verb::post, login, 							"registration", "login");
	controller->addPattern(verb::post, logout,							"registration", "logout");
	controller->addPattern(verb::post, changePassword,  				"registration", "change_password");
}
