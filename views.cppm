// FUZE.page 2026
// The following code is not to be used for AI training. For humans, the MIT license applies.
module;
// #include "views.hpp"
#include <boost/beast/http/status.hpp>
#include <boost/json.hpp>
#include <iostream>
#include <print>
#include "Request.hpp"
#include <unordered_set>
export module Mediaboard.Views;

import FuzeHttp.Core;
import FuzeHttp.PermissionObject;
import FuzeHttp.Utils;
import Mediaboard.Board;
import Mediaboard.Permission;
import Mediaboard.State;
import Mediaboard.Thread;

using namespace FuzeHttp;
export namespace Mediaboard {

FuzeHttp::Response showDocument(Mediaboard::State* state, FuzeHttp::Request req) {
	// TODO handle target decoding in FuzeHttp
	std::string target = std::string(FuzeHttp::getPathName(FuzeHttp::getDecodedURL(req.target())).substr(1));
	return {
		.status = http::status::ok,
		// .headers = {return_headers},
		.file = state->getDocumentRoot() / target
	};
}

// FuzeHttp::Response showBoardPage(Mediaboard::State* state, FuzeHttp::Request req, std::string board_slug) {
// 	std::string target = std::string(FuzeHttp::getPathName(FuzeHttp::getDecodedURL(req.target())).substr(1));
// 	return {
// 		.status = http::status::ok,
// 		// .headers = {return_headers},
// 		.file = state->getDocumentRoot() / "index.html"
// 	};
// }

FuzeHttp::Response createBoard(Mediaboard::State* state, FuzeHttp::Request req, Client client) {
	bool make_public;
	boost::json::object board_json;
	std::string new_slug, new_title;
	try {
		boost::json::object req_json = boost::json::parse(req.body()).as_object();
		board_json = req_json.at("board").as_object();
		new_slug = board_json.at("slug").as_string();
		new_title = board_json.at("title").as_string();
		make_public = req_json.at("make_public").as_bool();
	}
	catch(const std::exception& e) {
		std::string error_message = std::format("JSON error: {}", e.what());
		std::cerr << error_message << std::endl;
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = error_message};
	}
	if (!state->clientHasPermission(client, static_cast<int>(PERMISSION::CREATE_BOARD))) {
		return FuzeHttp::Response{
			.status = http::status::forbidden,
			.error_message = std::string("Client lacks permission CREATE_BOARD.")
		};
	}
	if (new_slug.length() < 1 || new_slug.length() > Board::MAX_SLUG)
		return Response{.status = http::status::bad_request, .error_message = std::format("Slug length {} is not between 1 and {}", new_slug.length(), Board::MAX_SLUG)};
	if (new_title.length() < 1 || new_title.length() > Board::MAX_TITLE)
		return Response{.status = http::status::bad_request, .error_message = std::format("Title length {} is not between 1 and {}", new_title.length(), Board::MAX_TITLE)};
	if (!isValidURLParameter(new_slug)) {
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = "Slug can only contain alphanumeric characters, '_', or '-'."};
	}
	if (state->getBoardIfExists(new_slug)) // Note: board slugs can be enumerated if client has permission to edit board. No real way to avoid this.
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = std::format("Board with slug {} already exists. Note: old slug associations are cleared on reboot.", new_slug)};
	Board* board = state->createBoard(board_json);
	if (make_public && !board->clientHasPermission({}, static_cast<int>(PERMISSION::VIEW_BOARD)))
		board->setGroupPermission(static_cast<int>(BUILTIN_GROUPS::PUBLIC), static_cast<int>(PERMISSION::VIEW_BOARD), THREE_STATE_SETTING::ALLOW);
	else if (!make_public && board->clientHasPermission({}, static_cast<int>(PERMISSION::VIEW_BOARD)))
		board->setGroupPermission(static_cast<int>(BUILTIN_GROUPS::PUBLIC), static_cast<int>(PERMISSION::VIEW_BOARD), THREE_STATE_SETTING::DENY);

	return FuzeHttp::Response{
		.status = http::status::created
		// .headers = {{
		// 	{"New-Thread-Id", std::to_string(new_thread_id)}
		// 	// ,{"Location", std::format("/thread/{}/", new_thread_id)}
		// }}
	};
}

FuzeHttp::Response getBoards(Mediaboard::State* state, FuzeHttp::Request req) {
	std::optional<Client> client = state->getClientIfExists(req);
	std::println("called getBoards");
	return Response{
		.status = http::status::ok,
		.json = state->getBoardsAsJson(client)
	};
}

FuzeHttp::Response getBoard(Mediaboard::State* state, FuzeHttp::Request req, std::string board_slug) {
	std::optional<Client> client = state->getClientIfExists(req);
	std::optional<Board*> board = state->getBoardIfExistsAndClientHasReadPermission(board_slug, client);
	if (!board)
		return Response{.status = http::status::not_found, .error_message = std::format("Board '{}' either doesn't exist, or client lacks permission to access it.", board_slug)};
	return Response{
		.status = http::status::ok,
		.json = board.value()->asJson(client)
	};
}

// TODO update etag of index.html on edit to refresh cache
FuzeHttp::Response editBoard(Mediaboard::State* state, FuzeHttp::Request req, Client client, std::string board_slug) {
	bool make_public;
	boost::json::object board_json;
	std::string new_slug, new_title;
	try {
		boost::json::object req_json = boost::json::parse(req.body()).as_object();
		board_json = req_json.at("board").as_object();
		new_slug = board_json.at("slug").as_string();
		new_title = board_json.at("title").as_string();
	}
	catch(const std::exception& e) {
		std::string error_message = std::format("[editBoard] JSON error: {}", e.what());
		std::cerr << error_message << std::endl;
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = error_message};
	}
	std::optional<Board*> board = state->getBoardIfExistsAndClientHasReadPermission(board_slug, client);
	if (!board)
		return Response{.status = http::status::not_found, .error_message = std::format("Board '{}' either doesn't exist, or client lacks permission to access it.", board_slug)};
	if (!board.value()->clientHasPermission(client, static_cast<int>(PERMISSION::CREATE_BOARD))) {
		return FuzeHttp::Response{
			.status = http::status::forbidden,
			.error_message = std::string("Client lacks permission CREATE_BOARD.")
		};
	}
	if (new_slug.length() < 1 || new_slug.length() > Board::MAX_SLUG)
		return Response{.status = http::status::bad_request, .error_message = std::format("Slug length {} is not between 1 and {}", new_slug.length(), Board::MAX_SLUG)};
	if (new_title.length() < 1 || new_title.length() > Board::MAX_TITLE)
		return Response{.status = http::status::bad_request, .error_message = std::format("Title length {} is not between 1 and {}", new_title.length(), Board::MAX_TITLE)};
	if (!isValidURLParameter(new_slug)) {
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = "Slug can only contain alphanumeric characters, '_', or '-'."};
	}
	if (state->getBoardIfExists(new_slug)) // Note: board slugs can be enumerated if client has permission to edit board. No real way to avoid this.
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = std::format("Board with slug {} already exists. Note: old slug associations are cleared on reboot.", new_slug)};
	state->setBoardSlug(board.value()->getId(), new_slug);
	board.value()->setTitle(new_title);
	return FuzeHttp::Response{
		.status = http::status::created
	};
}

FuzeHttp::Response deleteBoard(Mediaboard::State* state, FuzeHttp::Request req, Client client, std::string board_slug) {
	std::optional<Board*> board = state->getBoardIfExistsAndClientHasReadPermission(board_slug, client);
	if (!board)
		return Response{.status = http::status::not_found, .error_message = std::format("Board '{}' either doesn't exist, or client lacks permission to access it.", board_slug)};
	if (!board.value()->clientHasPermission(client, static_cast<int>(PERMISSION::DELETE_BOARD)))
		return FuzeHttp::Response{.status = http::status::forbidden, .error_message = "You lack permission to delete this board."};
	board.value()->markAsDeleted();
	return Response{
		.status = http::status::no_content
	};
}

FuzeHttp::Response getBoardPermissions(Mediaboard::State* state, FuzeHttp::Request req, std::string board_slug) {
	std::optional<Client> client = state->getClientIfExists(req);
	std::optional<Board*> board = state->getBoardIfExistsAndClientHasReadPermission(board_slug, client);
	if (!board)
		return Response{.status = http::status::not_found, .error_message = std::format("Board '{}' either doesn't exist, or client lacks permission to access it.", board_slug)};
	else if (!board.value()->clientHasPermission(client, static_cast<int>(PERMISSION::VIEW_THREAD)))
		return FuzeHttp::Response{.status = http::status::forbidden, .error_message = "You lack permission to view this thread."};
	return FuzeHttp::Response{
		.status = http::status::ok,
		.json = board.value()->getPermissionCollectionsAsJson()
	};
}

FuzeHttp::Response addBoardGroupPermission(Mediaboard::State* state, FuzeHttp::Request req, std::string board_slug, int group_id) {
	std::optional<Client> client = state->getClientIfExists(req);
	std::optional<Board*> board = state->getBoardIfExistsAndClientHasReadPermission(board_slug, client);
	if (!board)
		return Response{.status = http::status::not_found, .error_message = std::format("Board '{}' either doesn't exist, or client lacks permission to access it.", board_slug)};
	if (!board.value()->clientHasPermissionForGroup(client, static_cast<int>(PERMISSION::MANAGE_PERMISSIONS), group_id))
		return FuzeHttp::Response{.status = http::status::forbidden, .error_message = "You lack permission manage permissions for this board."};
	else if (board.value()->permissionCollectionExistsForGroup(group_id))
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = "Permissions for this group are already set. Use PUT request instead."};
	board.value()->addGroupPermissionCollection(group_id);
	return FuzeHttp::Response{
		.status = http::status::created
	};
}

FuzeHttp::Response addBoardUserPermission(Mediaboard::State* state, FuzeHttp::Request req, std::string board_slug, int account_id) {
	std::optional<Client> client = state->getClientIfExists(req);
	std::optional<Board*> board = state->getBoardIfExistsAndClientHasReadPermission(board_slug, client);
	if (!board)
		return Response{.status = http::status::not_found, .error_message = std::format("Board '{}' either doesn't exist, or client lacks permission to access it.", board_slug)};
	if (!board.value()->clientHasPermissionForAccount(client, static_cast<int>(PERMISSION::MANAGE_PERMISSIONS), account_id))
		return FuzeHttp::Response{.status = http::status::forbidden, .error_message = "You lack permission manage permissions for this thread."};
	else if (board.value()->permissionCollectionExistsForAccount(account_id))
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = "Permissions for this account are already set. Use PUT request instead."};
	board.value()->addAccountPermissionCollection(account_id);
	return FuzeHttp::Response{
		.status = http::status::created
	};
}

FuzeHttp::Response updateBoardGroupPermissions(Mediaboard::State* state, FuzeHttp::Request req, std::string board_slug, int group_id) {
	std::optional<Client> client = state->getClientIfExists(req);
	std::optional<Board*> board = state->getBoardIfExistsAndClientHasReadPermission(board_slug, client);
	if (!board)
		return Response{.status = http::status::not_found, .error_message = std::format("Board '{}' either doesn't exist, or client lacks permission to access it.", board_slug)};
	boost::json::object request_json;
	int permission_number, permission_setting;
	try {
		request_json = boost::json::parse(req.body()).as_object();
		permission_number = request_json["permission"].as_int64();
		permission_setting = request_json["setting"].as_int64();
	}
	catch(const std::exception& e) {
		std::string error_message = std::format("[updateBoardGroupPermissions] JSON error: {}", e.what());
		std::cerr << error_message << std::endl;
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = error_message};
	}
	if (permission_number < 0 || permission_number >= static_cast<int>(PERMISSION::NUMBER_OF_PERMISSIONS))
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = "Invalid permission number in JSON"};
	if (permission_setting < 0 || permission_setting >= 3)
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = "Invalid permission setting in JSON"};
	if (!board.value()->clientHasPermissionForGroup(client, static_cast<int>(PERMISSION::MANAGE_PERMISSIONS), group_id))
		return FuzeHttp::Response{.status = http::status::forbidden, .error_message = "You lack permission to update permissions for this group."};
	board.value()->setGroupPermission(group_id, permission_number, static_cast<THREE_STATE_SETTING>(permission_setting));
	return FuzeHttp::Response{
		.status = http::status::ok
	};
}

FuzeHttp::Response updateBoardUserPermissions(Mediaboard::State* state, FuzeHttp::Request req, std::string board_slug, int account_id) {
	std::optional<Client> client = state->getClientIfExists(req);
	std::optional<Board*> board = state->getBoardIfExistsAndClientHasReadPermission(board_slug, client);
	if (!board)
		return Response{.status = http::status::not_found, .error_message = std::format("Board '{}' either doesn't exist, or client lacks permission to access it.", board_slug)};
	boost::json::object request_json;
	int permission_number, permission_setting;
	try {
		request_json = boost::json::parse(req.body()).as_object();
		permission_number = request_json["permission"].as_int64();
		permission_setting = request_json["setting"].as_int64();
	}
	catch(const std::exception& e) {
		std::string error_message = std::format("[updateBoardUserPermissions] JSON error: {}", e.what());
		std::cerr << error_message << std::endl;
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = error_message};
	}
	if (permission_number < 0 || permission_number >= static_cast<int>(PERMISSION::NUMBER_OF_PERMISSIONS))
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = "Invalid permission number in JSON"};
	if (permission_setting < 0 || permission_setting >= 3)
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = "Invalid permission setting in JSON"};
	if (!board.value()->clientHasPermissionForAccount(client, static_cast<int>(PERMISSION::MANAGE_PERMISSIONS), account_id))
		return FuzeHttp::Response{.status = http::status::forbidden, .error_message = "You lack permission to update permissions for this account."};
	board.value()->setAccountPermission(account_id, permission_number, static_cast<THREE_STATE_SETTING>(permission_setting));
	return FuzeHttp::Response{
		.status = http::status::ok
	};
}

FuzeHttp::Response deleteBoardGroupPermission(Mediaboard::State* state, FuzeHttp::Request req, std::string board_slug, int group_id) {
	std::optional<Client> client = state->getClientIfExists(req);
	std::optional<Board*> board = state->getBoardIfExistsAndClientHasReadPermission(board_slug, client);
	if (!board)
		return Response{.status = http::status::not_found, .error_message = std::format("Board '{}' either doesn't exist, or client lacks permission to access it.", board_slug)};
	if (!board.value()->permissionCollectionExistsForGroup(group_id))
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = "No permissions set for this group."};
	if (!board.value()->clientHasPermissionForGroup(client, static_cast<int>(PERMISSION::MANAGE_PERMISSIONS), group_id))
		return FuzeHttp::Response{.status = http::status::forbidden, .error_message = "You lack permission to manage permissions for this group."};
	board.value()->removeGroupPermissionCollection(group_id);
	return FuzeHttp::Response{
		.status = http::status::ok
	};
}

FuzeHttp::Response deleteBoardUserPermission(Mediaboard::State* state, FuzeHttp::Request req, std::string board_slug, int account_id) {
	std::optional<Client> client = state->getClientIfExists(req);
	std::optional<Board*> board = state->getBoardIfExistsAndClientHasReadPermission(board_slug, client);
	if (!board)
		return Response{.status = http::status::not_found, .error_message = std::format("Board '{}' either doesn't exist, or client lacks permission to access it.", board_slug)};
	if (!board.value()->permissionCollectionExistsForAccount(account_id))
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = "No permissions set for this account."};
	if (!board.value()->clientHasPermissionForAccount(client, static_cast<int>(PERMISSION::MANAGE_PERMISSIONS), account_id))
		return FuzeHttp::Response{.status = http::status::forbidden, .error_message = "You lack permission to manage permissions for this account."};
	board.value()->removeAccountPermissionCollection(account_id);
	return FuzeHttp::Response{
		.status = http::status::ok
	};
}

FuzeHttp::Response createThread(Mediaboard::State* state, FuzeHttp::Request req, Client client, std::string board_slug) {
	boost::json::object thread_json;
	try {
		thread_json = boost::json::parse(req.body()).at("thread").as_object();
	}
	catch(const std::exception& e) {
		std::string error_message = std::format("[createThread] JSON error: {}", e.what());
		std::cerr << error_message << std::endl;
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = error_message};
	}
	if (!state->clientHasPermission(client, static_cast<int>(PERMISSION::CREATE_THREAD))) {
		return FuzeHttp::Response{
			.status = http::status::forbidden,
			.error_message = std::string("Client lacks permission CREATE_THREAD.")
		};
	}
	std::optional<Board*> board = state->getBoardIfExistsAndClientHasReadPermission(board_slug, client);
	if (!board)
		return Response{.status = http::status::not_found, .error_message = std::format("Board '{}' either doesn't exist, or client lacks permission to access it.", board_slug)};
	int new_thread_id = board.value()->createThread(thread_json, client.id);

	return FuzeHttp::Response{
		.status = http::status::created,
		.headers = {{
			{"New-Thread-Id", std::to_string(new_thread_id)}
			// ,{"Location", std::format("/thread/{}/", new_thread_id)}
		}}
	};
}

FuzeHttp::Response createMessage(Mediaboard::State* state, FuzeHttp::Request req, Client client, std::string board_slug) {
	boost::json::object message_json;
	int thread_id;
	try {
		message_json = boost::json::parse(req.body()).at("post").as_object();
		thread_id = message_json.at("thread_id").as_int64();
	}
	catch(const std::exception& e) {
		std::string error_message = std::format("[createMessage] JSON error: {}", e.what());
		std::cerr << error_message << std::endl;
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = error_message};
	}
	std::optional<Board*> board = state->getBoardIfExistsAndClientHasReadPermission(board_slug, client);
	if (!board)
		return Response{.status = http::status::not_found, .error_message = std::format("Board '{}' either doesn't exist, or client lacks permission to access it.", board_slug)};
	if (!board.value()->threadExists(thread_id))
		return FuzeHttp::Response{.status = http::status::not_found, .error_message = "This thread was not found."};
	if (!board.value()->getThread(thread_id)->clientHasPermission(client, static_cast<int>(PERMISSION::SEND_MESSAGE)))
		return FuzeHttp::Response{.status = http::status::forbidden, .error_message = "User lacks permission SEND_MESSAGE within this thread"};
	int new_message_id = board.value()->createMessage(message_json, client.id);
	std::string new_message_dump = board.value()->dumpMessage(thread_id, new_message_id);
	state->sendToThread(new_message_dump, board.value(), thread_id);
	return FuzeHttp::Response{
		.status = http::status::created
	};
}

FuzeHttp::Response deleteMessage(Mediaboard::State* state, FuzeHttp::Request req, Client client, std::string board_slug, int thread_id, int message_id_in_thread) {
	std::optional<Board*> board = state->getBoardIfExistsAndClientHasReadPermission(board_slug, client);
	if (!board)
		return Response{.status = http::status::not_found, .error_message = std::format("Board '{}' either doesn't exist, or client lacks permission to access it.", board_slug)};
	if (!board.value()->threadExists(thread_id))
		return FuzeHttp::Response{.status = http::status::not_found, .error_message = "This thread was not found."};
	const Thread* thread = board.value()->getThread(thread_id);
	if (!thread->messageExists(message_id_in_thread))
		return FuzeHttp::Response{.status = http::status::not_found, .error_message = "No such message found in this thread."};
	if (!thread->clientHasPermission(client, static_cast<int>(PERMISSION::DELETE_POST)) && !thread->getMessage(message_id_in_thread)->clientIsAuthor(client))
		return FuzeHttp::Response{.status = http::status::forbidden, .error_message = "You lack permission to delete this message."};
	if (message_id_in_thread == 0)
		board.value()->deleteThread(thread_id);
	else
		board.value()->deleteMessageFromThread(message_id_in_thread, thread_id);
	return FuzeHttp::Response{
		.status = http::status::ok
	};
}

FuzeHttp::Response getThread(Mediaboard::State* state, FuzeHttp::Request req, std::string board_slug, int thread_id) {
	std::optional<Client> client = state->getClientIfExists(req);
	std::optional<Board*> board = state->getBoardIfExistsAndClientHasReadPermission(board_slug, client);
	if (!board)
		return Response{.status = http::status::not_found, .error_message = std::format("Board '{}' either doesn't exist, or client lacks permission to access it.", board_slug)};
	if (!board.value()->threadExists(thread_id))
		return FuzeHttp::Response{.status = http::status::not_found, .error_message = "This thread was not found."};
	const Thread* thread = board.value()->getThread(thread_id);
	if (thread->isDeleted())
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = "This thread has been deleted"};
	if (!thread->clientHasPermission(client, static_cast<int>(PERMISSION::VIEW_THREAD)))
		return FuzeHttp::Response{.status = http::status::forbidden, .error_message = "You lack permission to view this thread."};
	return FuzeHttp::Response{
		.status = http::status::ok,
		.json = board.value()->getThread(thread_id)->asJsonWithMessages(client)
	};
}

FuzeHttp::Response getThreadPermissions(Mediaboard::State* state, FuzeHttp::Request req, std::string board_slug, int thread_id) {
	std::optional<Client> client = state->getClientIfExists(req);
	std::optional<Board*> board = state->getBoardIfExistsAndClientHasReadPermission(board_slug, client);
	if (!board)
		return Response{.status = http::status::not_found, .error_message = std::format("Board '{}' either doesn't exist, or client lacks permission to access it.", board_slug)};
	if (!board.value()->threadExists(thread_id))
		return FuzeHttp::Response{.status = http::status::not_found, .error_message = "This thread was not found."};
	else if (!board.value()->getThread(thread_id)->clientHasPermission(client, static_cast<int>(PERMISSION::VIEW_THREAD)))
		return FuzeHttp::Response{.status = http::status::forbidden, .error_message = "You lack permission to view this thread."};
	return FuzeHttp::Response{
		.status = http::status::ok,
		.json = board.value()->getThread(thread_id)->getPermissionCollectionsAsJson()
	};
}

FuzeHttp::Response addThreadGroupPermission(Mediaboard::State* state, FuzeHttp::Request req, std::string board_slug, int thread_id, int group_id) {
	std::optional<Client> client = state->getClientIfExists(req);
	std::optional<Board*> board = state->getBoardIfExistsAndClientHasReadPermission(board_slug, client);
	if (!board)
		return Response{.status = http::status::not_found, .error_message = std::format("Board '{}' either doesn't exist, or client lacks permission to access it.", board_slug)};
	const Thread* thread = board.value()->getThread(thread_id);
	if (!thread->clientHasPermissionForGroup(client, static_cast<int>(PERMISSION::MANAGE_PERMISSIONS), group_id))
		return FuzeHttp::Response{.status = http::status::forbidden, .error_message = "You lack permission manage permissions for this thread."};
	else if (thread->permissionCollectionExistsForGroup(group_id))
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = "Permissions for this group are already set. Use PUT request instead."};
	board.value()->addGroupPermissionCollectionToThread(group_id, thread_id);
	return FuzeHttp::Response{
		.status = http::status::created
	};
}

FuzeHttp::Response addThreadUserPermission(Mediaboard::State* state, FuzeHttp::Request req, std::string board_slug, int thread_id, int account_id) {
	std::optional<Client> client = state->getClientIfExists(req);
	std::optional<Board*> board = state->getBoardIfExistsAndClientHasReadPermission(board_slug, client);
	if (!board)
		return Response{.status = http::status::not_found, .error_message = std::format("Board '{}' either doesn't exist, or client lacks permission to access it.", board_slug)};
	const Thread* thread = board.value()->getThread(thread_id);
	if (!thread->clientHasPermissionForAccount(client, static_cast<int>(PERMISSION::MANAGE_PERMISSIONS), account_id))
		return FuzeHttp::Response{.status = http::status::forbidden, .error_message = "You lack permission manage permissions for this thread."};
	else if (thread->permissionCollectionExistsForAccount(account_id))
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = "Permissions for this account are already set. Use PUT request instead."};
	board.value()->addAccountPermissionCollectionToThread(account_id, thread_id);
	return FuzeHttp::Response{
		.status = http::status::created
	};
}

FuzeHttp::Response updateThreadGroupPermissions(Mediaboard::State* state, FuzeHttp::Request req, std::string board_slug, int thread_id, int group_id) {
	std::optional<Client> client = state->getClientIfExists(req);
	std::optional<Board*> board = state->getBoardIfExistsAndClientHasReadPermission(board_slug, client);
	if (!board)
		return Response{.status = http::status::not_found, .error_message = std::format("Board '{}' either doesn't exist, or client lacks permission to access it.", board_slug)};
	const Thread* thread = board.value()->getThread(thread_id);
	boost::json::object request_json;
	int permission_number, permission_setting;
	try {
		request_json = boost::json::parse(req.body()).as_object();
		permission_number = request_json["permission"].as_int64();
		permission_setting = request_json["setting"].as_int64();
	}
	catch(const std::exception& e) {
		std::string error_message = std::format("[updateThreadGroupPermissions] JSON error: {}", e.what());
		std::cerr << error_message << std::endl;
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = error_message};
	}
	if (permission_number < 0 || permission_number >= static_cast<int>(PERMISSION::NUMBER_OF_PERMISSIONS))
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = "Invalid permission number in JSON"};
	if (permission_setting < 0 || permission_setting >= 3)
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = "Invalid permission setting in JSON"};
	if (!thread->clientHasPermissionForGroup(client, static_cast<int>(PERMISSION::MANAGE_PERMISSIONS), group_id))
		return FuzeHttp::Response{.status = http::status::forbidden, .error_message = "You lack permission to update permissions for this group."};
	board.value()->setGroupPermissionForThread(group_id, permission_number, static_cast<THREE_STATE_SETTING>(permission_setting), thread_id);
	return FuzeHttp::Response{
		.status = http::status::ok
	};
}

FuzeHttp::Response updateThreadUserPermissions(Mediaboard::State* state, FuzeHttp::Request req, std::string board_slug, int thread_id, int account_id) {
	std::optional<Client> client = state->getClientIfExists(req);
	std::optional<Board*> board = state->getBoardIfExistsAndClientHasReadPermission(board_slug, client);
	if (!board)
		return Response{.status = http::status::not_found, .error_message = std::format("Board '{}' either doesn't exist, or client lacks permission to access it.", board_slug)};
	const Thread* thread = board.value()->getThread(thread_id);
	boost::json::object request_json;
	int permission_number, permission_setting;
	try {
		request_json = boost::json::parse(req.body()).as_object();
		permission_number = request_json["permission"].as_int64();
		permission_setting = request_json["setting"].as_int64();
	}
	catch(const std::exception& e) {
		std::string error_message = std::format("[updateThreadUserPermissions] JSON error: {}", e.what());
		std::cerr << error_message << std::endl;
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = error_message};
	}
	if (permission_number < 0 || permission_number >= static_cast<int>(PERMISSION::NUMBER_OF_PERMISSIONS))
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = "Invalid permission number in JSON"};
	if (permission_setting < 0 || permission_setting >= 3)
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = "Invalid permission setting in JSON"};
	if (!thread->clientHasPermissionForAccount(client, static_cast<int>(PERMISSION::MANAGE_PERMISSIONS), account_id))
		return FuzeHttp::Response{.status = http::status::forbidden, .error_message = "You lack permission to update permissions for this account."};
	board.value()->setAccountPermissionForThread(account_id, permission_number, static_cast<THREE_STATE_SETTING>(permission_setting), thread_id);
	return FuzeHttp::Response{
		.status = http::status::ok
	};
}

FuzeHttp::Response deleteThreadGroupPermission(Mediaboard::State* state, FuzeHttp::Request req, std::string board_slug, int thread_id, int group_id) {
	std::optional<Client> client = state->getClientIfExists(req);
	std::optional<Board*> board = state->getBoardIfExistsAndClientHasReadPermission(board_slug, client);
	if (!board)
		return Response{.status = http::status::not_found, .error_message = std::format("Board '{}' either doesn't exist, or client lacks permission to access it.", board_slug)};
	if (!board.value()->threadExists(thread_id))
		return FuzeHttp::Response{.status = http::status::not_found, .error_message = "This thread was not found."};
	const Thread* thread = board.value()->getThread(thread_id);
	if (!thread->permissionCollectionExistsForGroup(group_id))
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = "No permissions set for this group."};
	if (!thread->clientHasPermissionForGroup(client, static_cast<int>(PERMISSION::MANAGE_PERMISSIONS), group_id))
		return FuzeHttp::Response{.status = http::status::forbidden, .error_message = "You lack permission to manage permissions for this group."};
	board.value()->removeGroupPermissionCollectionFromThread(group_id, thread_id);
	return FuzeHttp::Response{
		.status = http::status::ok
	};
}

FuzeHttp::Response deleteThreadUserPermission(Mediaboard::State* state, FuzeHttp::Request req, std::string board_slug, int thread_id, int account_id) {
	std::optional<Client> client = state->getClientIfExists(req);
	std::optional<Board*> board = state->getBoardIfExistsAndClientHasReadPermission(board_slug, client);
	if (!board)
		return Response{.status = http::status::not_found, .error_message = std::format("Board '{}' either doesn't exist, or client lacks permission to access it.", board_slug)};
	if (!board.value()->threadExists(thread_id))
		return FuzeHttp::Response{.status = http::status::not_found, .error_message = "This thread was not found."};
	const Thread* thread = board.value()->getThread(thread_id);
	if (!thread->permissionCollectionExistsForAccount(account_id))
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = "No permissions set for this account."};
	if (!thread->clientHasPermissionForAccount(client, static_cast<int>(PERMISSION::MANAGE_PERMISSIONS), account_id))
		return FuzeHttp::Response{.status = http::status::forbidden, .error_message = "You lack permission to manage permissions for this account."};
	board.value()->removeAccountPermissionCollectionFromThread(account_id, thread_id);
	return FuzeHttp::Response{
		.status = http::status::ok
	};
}

FuzeHttp::Response getThreads(Mediaboard::State* state, FuzeHttp::Request req, std::string board_slug) {
	std::optional<Client> client = state->getClientIfExists(req);
	std::println("called getThreads");
	std::optional<Board*> board = state->getBoardIfExistsAndClientHasReadPermission(board_slug, client);
	if (!board)
		return Response{.status = http::status::not_found, .error_message = std::format("Board '{}' either doesn't exist, or client lacks permission to access it.", board_slug)};
	if (!board.value()->clientHasPermission(client, static_cast<int>(PERMISSION::VIEW_BOARD)))
		return FuzeHttp::Response{.status = http::status::forbidden, .error_message = "You lack permission to view this board."};
	return Response{
		.status = http::status::ok,
		.json = board.value()->getThreadsAsJson(client)
	};
}

FuzeHttp::Response createGroup(Mediaboard::State* state, FuzeHttp::Request req) {
	std::optional<Client> client = state->getClientIfExists(req);
	std::print("Client rank: {}", state->getClientRank(client));
	std::print("Ordered_groups: {}", state->getOrderedGroups()->size());
	if (!state->clientHasPermission(client, static_cast<int>(PERMISSION::MANAGE_PERMISSIONS)))
		return FuzeHttp::Response{.status = http::status::forbidden, .error_message = "Client lacks permission MANAGE_PERMISSIONS"};
	else if (state->getClientRank(client) >= state->getOrderedGroups()->size() - 2)
		return FuzeHttp::Response{.status = http::status::forbidden, .error_message = "Only users within a group with rank above \"Account\" can create groups"};
	boost::json::object group_json;
	std::string new_group_name;
	try {
		group_json = boost::json::parse(req.body()).at("group").as_object();
		new_group_name = group_json.at("name").as_string();
	}
	catch(const std::exception& e) {
		std::string error_message = std::format("[createGroup] JSON error: {}", e.what());
		std::cerr << error_message << std::endl;
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = error_message};
	}
	int new_group_rank = state->getClientRank(client) + 1;
	/*int new_group_id = */state->addGroup(new_group_name, new_group_rank);
	return FuzeHttp::Response{
		.status = http::status::created
	};
}

FuzeHttp::Response deleteGroup(Mediaboard::State* state, FuzeHttp::Request req, int group_id) {
	std::optional<Client> client = state->getClientIfExists(req);
	if (!state->groupExists(group_id))
		return FuzeHttp::Response{.status = http::status::not_found, .error_message = "Group does not exist."};
	if (!state->clientHasPermissionForGroup(client, static_cast<int>(PERMISSION::MANAGE_PERMISSIONS), group_id))
		return FuzeHttp::Response{.status = http::status::forbidden, .error_message = "You lack permission to delete this group."};
	state->eraseGroup(group_id);
	return FuzeHttp::Response{
		.status = http::status::ok
	};
}

FuzeHttp::Response removeMemberFromGroup(Mediaboard::State* state, FuzeHttp::Request req, int group_id, int account_id) {
	std::optional<Client> client = state->getClientIfExists(req);
	if (!state->groupExists(group_id))
		return FuzeHttp::Response{.status = http::status::not_found, .error_message = "Group does not exist."};
	if (!state->clientHasPermissionForGroup(client, static_cast<int>(PERMISSION::MANAGE_PERMISSIONS), group_id))
		return FuzeHttp::Response{.status = http::status::forbidden, .error_message = "You lack permission to delete this group."};
	if (!state->accountExists(account_id))
		return FuzeHttp::Response{.status = http::status::not_found, .error_message = std::format("Account {} not found.", account_id)};
	if (!state->clientHasPermissionForAccount(client, static_cast<int>(PERMISSION::MANAGE_PERMISSIONS), account_id))
		return FuzeHttp::Response{.status = http::status::forbidden, .error_message = "You lack permission to remove this account from a group."};
	state->removeUserFromGroup(account_id, group_id);
	return FuzeHttp::Response{
		.status = http::status::ok
	};
}

FuzeHttp::Response getGroupMembers(Mediaboard::State* state, FuzeHttp::Request req, int group_id) {
	if (!state->groupExists(group_id))
		return FuzeHttp::Response{.status = http::status::not_found, .error_message = "Group not found."};
	return FuzeHttp::Response{
		.status = http::status::ok,
		.json = state->getGroupMembersAsJson(group_id)
	};
}

FuzeHttp::Response getGroups(Mediaboard::State* state, FuzeHttp::Request req) {
	std::optional<Client> client = state->getClientIfExists(req);
	return FuzeHttp::Response{
		.status = http::status::ok,
		.body = state->dumpAllGroups(client)
	};
}

FuzeHttp::Response setGroupHeirarchy(Mediaboard::State* state, FuzeHttp::Request req) {
	std::optional<Client> client = state->getClientIfExists(req);
	boost::json::object request_json;
	std::vector<int> new_group_heirarchy;
	try {
		request_json = boost::json::parse(req.body()).as_object();
		for(auto group : request_json.at("new_group_heirarchy").as_array()) {
			new_group_heirarchy.push_back(group.as_int64());
		}
	}
	catch(const std::exception& e) {
		std::string error_message = std::format("[setGroupHeirarchy] JSON error: {}", e.what());
		std::cerr << error_message << std::endl;
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = error_message};
	}
	if (!state->clientHasPermission(client, static_cast<int>(PERMISSION::MANAGE_PERMISSIONS)))
		return FuzeHttp::Response{.status = http::status::forbidden, .error_message = "Client lacks permission MANAGE_PERMISSIONS"};
	if (new_group_heirarchy.size() != state->getOrderedGroups()->size())
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = "[setGroupHeirarchy] Number of groups does not match"};

	int client_rank = state->getClientRank(client);
	std::unordered_set<int> new_group_order_set;
	// for (const int group_id : *(this->getOrderedGroups())) {
	for (int group_rank = 0; group_rank < new_group_heirarchy.size(); group_rank++) {
		int group_id = new_group_heirarchy[group_rank];
		std::cout << group_id << "G : ";
		// Check for duplicates
		if (new_group_order_set.contains(group_id))
			return FuzeHttp::Response{.status = http::status::bad_request, .error_message = std::format("Duplicate group {} detected", group_id)};

		new_group_order_set.insert(group_id);

		// Check if all groups exist
		if (!state->groupExists(group_id)) {
			return FuzeHttp::Response{.status = http::status::not_found, .error_message = "Group not found."};
		}
		// Check if user rank is high enough to change this group's rank
		int existing_group_at_this_rank = (*(state->getOrderedGroups()))[group_rank];
		std::cout << existing_group_at_this_rank << std::endl;
		if (group_rank <= client_rank && group_id != existing_group_at_this_rank)
			return FuzeHttp::Response{.status = http::status::forbidden, .error_message = "Permission denied; attempted to change order of groups greater than or equal to your rank."};
	}
	if (new_group_heirarchy[0] != static_cast<int>(BUILTIN_GROUPS::OWNER) ||
		new_group_heirarchy[new_group_heirarchy.size()-2] != static_cast<int>(BUILTIN_GROUPS::USERS) ||
		new_group_heirarchy[new_group_heirarchy.size()-1] != static_cast<int>(BUILTIN_GROUPS::PUBLIC))
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = "Attempted to change heirarchy of locked groups"};

	// this->ordered_groups_vec = ordered_groups;
	state->setOrderedGroups(new_group_heirarchy);
	return FuzeHttp::Response{
		.status = http::status::ok
	};
}

FuzeHttp::Response addGroupsToUser(Mediaboard::State* state, FuzeHttp::Request req, int account_id) {
	std::optional<Client> client = state->getClientIfExists(req);
	boost::json::object request_json;
	std::vector<int> groups_to_add;
	try {
		request_json = boost::json::parse(req.body()).as_object();
		for(auto group : request_json.at("groups_by_id").as_array()) {
			groups_to_add.push_back(group.as_int64());
		}
	}
	catch(const std::exception& e) {
		std::string error_message = std::format("[createMessage] JSON error: {}", e.what());
		std::cerr << error_message << std::endl;
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = error_message};
	}
	if (!state->clientHasPermission(client, static_cast<int>(PERMISSION::MANAGE_PERMISSIONS)))
		return FuzeHttp::Response{.status = http::status::forbidden, .error_message = "Cannot change group heirarchy; permission denied."};
	if (!state->accountExists(account_id))
		return FuzeHttp::Response{.status = http::status::not_found, .error_message = std::format("Account {} not found.", account_id)};
	int client_rank = state->getClientRank(client);
	for (int group_id : groups_to_add) {
		if (client_rank >= state->getGroupRank(group_id))
			return FuzeHttp::Response{.status = http::status::forbidden, .error_message = std::format("You do not have permission for group {}.", group_id)};
	}
	for (int group_id : groups_to_add) {
		state->addAccountToGroup(account_id, group_id);
	}
	return FuzeHttp::Response{
		.status = http::status::ok,
	};
}

FuzeHttp::Response getServerPermissions(Mediaboard::State* state, FuzeHttp::Request req) {
	return FuzeHttp::Response{
		.status = http::status::ok,
		.json = state->getPermissionCollectionsAsJson()
	};
}

FuzeHttp::Response addServerGroupPermission(Mediaboard::State* state, FuzeHttp::Request req, int group_id) {
	std::optional<Client> client = state->getClientIfExists(req);
	if (!state->clientHasPermissionForGroup(client, static_cast<int>(PERMISSION::MANAGE_PERMISSIONS), group_id))
		return FuzeHttp::Response{.status = http::status::forbidden, .error_message = "You lack permission to manage permissions."};
	else if (state->permissionCollectionExistsForGroup(group_id))
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = "Permissions for this group are already set. Use PUT request instead."};
	state->addGroupPermissionCollection(group_id);
	return FuzeHttp::Response{
		.status = http::status::created
	};
}

FuzeHttp::Response addServerUserPermission(Mediaboard::State* state, FuzeHttp::Request req, int account_id) {
	std::optional<Client> client = state->getClientIfExists(req);
	if (!state->clientHasPermissionForAccount(client, static_cast<int>(PERMISSION::MANAGE_PERMISSIONS), account_id))
		return FuzeHttp::Response{.status = http::status::forbidden, .error_message = "You lack permission to manage permissions."};
	else if (state->permissionCollectionExistsForAccount(account_id))
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = "Permissions for this account are already set. Use PUT request instead."};
	state->addAccountPermissionCollection(account_id);
	return FuzeHttp::Response{
		.status = http::status::created
	};
}

FuzeHttp::Response updateServerGroupPermissions(Mediaboard::State* state, FuzeHttp::Request req, int group_id) {
	std::optional<Client> client = state->getClientIfExists(req);
	boost::json::object request_json;
	int permission_number, permission_setting;
	try {
		request_json = boost::json::parse(req.body()).as_object();
		permission_number = request_json["permission"].as_int64();
		permission_setting = request_json["setting"].as_int64();
	}
	catch(const std::exception& e) {
		std::string error_message = std::format("[updateServerGroupPermissions] JSON error: {}", e.what());
		std::cerr << error_message << std::endl;
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = error_message};
	}
	if (permission_number < 0 || permission_number >= static_cast<int>(PERMISSION::NUMBER_OF_PERMISSIONS))
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = "Invalid permission number in JSON"};
	if (permission_setting < 0 || permission_setting >= 3)
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = "Invalid permission setting in JSON"};
	if (!state->clientHasPermissionForGroup(client, static_cast<int>(PERMISSION::MANAGE_PERMISSIONS), group_id))
		return FuzeHttp::Response{.status = http::status::forbidden, .error_message = "You lack permission to update permissions for this group."};
	state->setGroupPermission(group_id, permission_number, static_cast<THREE_STATE_SETTING>(permission_setting));
	return FuzeHttp::Response{
		.status = http::status::created
	};
}

FuzeHttp::Response updateServerUserPermissions(Mediaboard::State* state, FuzeHttp::Request req, int account_id) {
	std::optional<Client> client = state->getClientIfExists(req);
	boost::json::object request_json;
	int permission_number, permission_setting;
	try {
		request_json = boost::json::parse(req.body()).as_object();
		permission_number = request_json["permission"].as_int64();
		permission_setting = request_json["setting"].as_int64();
	}
	catch(const std::exception& e) {
		std::string error_message = std::format("[updateServerUserPermissions] JSON error: {}", e.what());
		std::cerr << error_message << std::endl;
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = error_message};
	}
	if (permission_number < 0 || permission_number >= static_cast<int>(PERMISSION::NUMBER_OF_PERMISSIONS))
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = "Invalid permission number in JSON"};
	if (permission_setting < 0 || permission_setting >= 3)
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = "Invalid permission setting in JSON"};
	if (!state->clientHasPermissionForAccount(client, static_cast<int>(PERMISSION::MANAGE_PERMISSIONS), account_id))
		return FuzeHttp::Response{.status = http::status::forbidden, .error_message = "You lack permission to update permissions for this account."};
	state->setAccountPermission(account_id, permission_number, static_cast<THREE_STATE_SETTING>(permission_setting));
	return FuzeHttp::Response{
		.status = http::status::created
	};
}

FuzeHttp::Response deleteServerGroupPermission(Mediaboard::State* state, FuzeHttp::Request req, int group_id) {
	std::optional<Client> client = state->getClientIfExists(req);
	if (!state->permissionCollectionExistsForGroup(group_id))
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = "This group does not exist."};
	if (!state->clientHasPermissionForGroup(client, static_cast<int>(PERMISSION::MANAGE_PERMISSIONS), group_id))
		return FuzeHttp::Response{.status = http::status::forbidden, .error_message = "You lack permission to delete this group."};
	state->removeGroupPermissionCollection(group_id);
	return FuzeHttp::Response{
		.status = http::status::ok
	};
}

FuzeHttp::Response deleteServerUserPermission(Mediaboard::State* state, FuzeHttp::Request req, int account_id) {
	std::optional<Client> client = state->getClientIfExists(req);
	if (!state->permissionCollectionExistsForAccount(account_id))
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = "This account does not exist."};
	if (!state->clientHasPermissionForAccount(client, static_cast<int>(PERMISSION::MANAGE_PERMISSIONS), account_id))
		return FuzeHttp::Response{.status = http::status::forbidden, .error_message = "You lack permission to delete this account."};
	state->removeAccountPermissionCollection(account_id);
	return FuzeHttp::Response{
		.status = http::status::ok
	};
}

FuzeHttp::Response client(Mediaboard::State* state, FuzeHttp::Request req) {
	std::optional<Client> client = state->getClientIfExists(req);
	// if (client) {
	// 	std::cout << "CLIENT FOUND ";
	// 	if (client.value().account_id)
	// 		std::cout << "ACCOUNT_ID FOUND ";
	// }
	return FuzeHttp::Response{
		.status = http::status::ok,
		.json = {{
			{"server_permissions", {
				{"manage_permissions", state->clientHasPermission(client, static_cast<int>(PERMISSION::MANAGE_PERMISSIONS))}
				// will be handled by Board-level perms
				// {"create_thread", state->clientHasPermission(client, static_cast<int>(PERMISSION::CREATE_THREAD))},
				// {"upload_file", state->clientHasPermission(client, static_cast<int>(PERMISSION::UPLOAD_FILE))}
			}},
			{"has_cookie", req.find("Cookie") != req.end()}
		}}
	};
}

FuzeHttp::Response getUsers(Mediaboard::State* state, FuzeHttp::Request req) {
	std::optional<Client> client = state->getClientIfExists(req);
	return FuzeHttp::Response{
		.status = http::status::ok,
		.headers = {{
			{"Client-Rank", std::to_string(state->getClientRank(client))}
		}},
		.body = state->dumpAllUsers(client)
	};
}

FuzeHttp::Response acceptInvite(Mediaboard::State* state, FuzeHttp::Request req, std::string invite_key_base64) {
	std::cout << "Checking invite link '" << invite_key_base64 << "'" << std::endl;
	// std::cout << "client ID is " << client.id << std::endl;
	int granted_group = state->getGrantedGroupIdFromInvite(invite_key_base64);
	if (granted_group == static_cast<int>(BUILTIN_GROUPS::PUBLIC)) {
		return FuzeHttp::Response{
			.status = http::status::bad_request,
			.body = "This invite link is invalid. It may have expired, or it might never had existed to begin with."
		};
	}
	return FuzeHttp::Response{
		.status = http::status::temporary_redirect,
		.headers = {{
			{"Location", std::format("/registration.html?invite={}", invite_key_base64)}
		}}
	};
}
} // namespace Mediaboard
