// Copyright (c) 2026, Fuze.page
// Fuze Human-oriented License v1
module;
#include <boost/beast/http.hpp>
export module Mediaboard.URLs;
import :media;
import :registration;

import FuzeHttp.Controller;
import Mediaboard.Permission;
import Mediaboard.Resolvers;
import Mediaboard.State;
import Mediaboard.Views;

using namespace FuzeHttp;
using namespace boost::beast::http;

export namespace Mediaboard {
void addURLsToController(FuzeHttp::Controller<State*>* controller) {
	// C-style strings are immutable parts of the URL, and strings/ints are variables passed into the view.
	// Client{} is used when the function needs to identify the user via a cookie.
	addMediaURLsToController(controller);
	addRegistrationURLsToController(controller);
	// board patterns
	controller->addPatterns()
	// (verb::get, showMainPage,						"boards")
	(verb::post, createBoard, Client{},				"api", "board")
	(verb::get, getBoards,							"api", "boards")
	(verb::get, getBoard, 							"api", "board", BoardResolver{})
	(verb::get, getBoardPermissions,				"api", "board", BoardResolver{}, "permissions")
	(verb::post, addBoardGroupPermission,			"api", "board", BoardResolver{}, "permissions", "group", int())
	(verb::put, updateBoardGroupPermissions,		"api", "board", BoardResolver{}, "permissions", "group", int())
	(verb::delete_, deleteBoardGroupPermission,		"api", "board", BoardResolver{}, "permissions", "group", int())
	(verb::post, addBoardUserPermission,			"api", "board", BoardResolver{}, "permissions", "user", int())
	(verb::put, updateBoardUserPermissions,			"api", "board", BoardResolver{}, "permissions", "user", int())
	(verb::delete_, deleteBoardUserPermission,		"api", "board", BoardResolver{}, "permissions", "user", int())
#ifdef WITH_WEBRTC
	// (verb::post, createRoom, Client{},			"api", "board", std::string{}, "room")
	(verb::get, getRoom,							"api", "board", BoardResolver{}, "room", int())
	(verb::get, getRooms,							"api", "board", BoardResolver{}, "rooms")
#endif
	(verb::post, createMessage, Client{},			"api", "board", BoardResolver{}, "thread", ThreadResolver<PERMISSION::SEND_MESSAGE>{}, "message")
	(verb::delete_, deleteMessage, Client{},		"api", "board", BoardResolver{}, "thread", ThreadResolver{}, "message", int())
	(verb::post, createThread, Client{},			"api", "board", BoardResolver<PERMISSION::CREATE_THREAD>{}, "thread")
	(verb::put, editBoard, Client{},				"api", "board", BoardResolver<PERMISSION::CREATE_BOARD>{})
	(verb::delete_, deleteBoard, Client{},			"api", "board", BoardResolver<PERMISSION::DELETE_BOARD>{})
	(verb::get, getThread, 							"api", "board", BoardResolver{}, "thread", ThreadResolver{})
	(verb::get, getThreadPermissions,				"api", "board", std::string{}, "thread", int(), "permissions")
	(verb::post, addThreadGroupPermission,			"api", "board", std::string{}, "thread", int(), "permissions", "group", int())
	(verb::put, updateThreadGroupPermissions,		"api", "board", std::string{}, "thread", int(), "permissions", "group", int())
	(verb::delete_, deleteThreadGroupPermission,	"api", "board", std::string{}, "thread", int(), "permissions", "group", int())
	(verb::post, addThreadUserPermission,			"api", "board", std::string{}, "thread", int(), "permissions", "user", int())
	(verb::put, updateThreadUserPermissions,		"api", "board", std::string{}, "thread", int(), "permissions", "user", int())
	(verb::delete_, deleteThreadUserPermission,		"api", "board", std::string{}, "thread", int(), "permissions", "user", int())
	(verb::get, getThreads, 						"api", "board", std::string{}, "threads")
	;
	// other
	controller->addPatterns()
	(verb::get, showDocument,						"*")
	(verb::post, createGroup,						"api", "create_group") // TODO move to server/permissions
	(verb::delete_, deleteGroup,					"api", "group", int())
	(verb::delete_, removeMemberFromGroup,			"api", "group", int(), "member", int())
	(verb::get, getGroupMembers,					"api", "group", int(), "members")
	(verb::get, getGroups,							"api", "groups")
	(verb::put, setGroupHeirarchy,					"api", "group_heirarchy")
#ifdef WITH_WEBRTC
	(verb::get, getIceServers,						"api", "ice_servers")
	(verb::put, updateIceServers,					"api", "ice_servers")
#endif
	(verb::get, getServerPermissions,				"api", "server", "permissions")
	(verb::post, addServerGroupPermission,			"api", "server", "permissions", "group", int())
	(verb::put, updateServerGroupPermissions,		"api", "server", "permissions", "group", int())
	(verb::delete_, deleteServerGroupPermission,	"api", "server", "permissions", "group", int())
	(verb::post, addServerUserPermission,			"api", "server", "permissions", "user", int())
	(verb::put, updateServerUserPermissions,		"api", "server", "permissions", "user", int())
	(verb::delete_, deleteServerUserPermission,		"api", "server", "permissions", "user", int())
	(verb::post, addGroupsToUser,					"api", "user", int(), "add_groups") // TODO move to server/permissions
	(verb::get, client, 							"api", "user", "client")
	(verb::get, getUsers,							"api", "users")
	(verb::get, acceptInvite,						"invite", std::string())
	;
}
} // export namespace Mediaboard
