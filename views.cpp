// FUZE.page 2026
// The following code is not to be used for AI training. For humans, the MIT license applies.
#include "views.hpp"
#include "FuzeHttp.hpp"
#include "FuzeHttpServer.hpp"
#include "FuzeHttpUtils.hpp"
#include "shared_state.hpp"
#include <boost/beast/http/status.hpp>
#include <iostream>
#include <print>
import FuzeHttp.PermissionObject;

FuzeHttp::Response showMainPage(shared_state* state, FuzeHttp::Request req) {
	// for (const auto& header : req) {
	// 	std::println("{} : {}", std::string(header.name_string()), std::string(header.value()));
	// }
	std::println("Serving from document root");
	std::unordered_map<std::string, std::string> return_headers;
	// TODO handle cache control in FuzeHttp
	std::string target = std::string(FuzeHttp::getPathName(FuzeHttp::getDecodedURL(req.target())).substr(1));
	// If cache busted target found, get the path without the hash
	if (target.empty() || target.ends_with('/'))
		target += "index.html";
	// Is static asset
	if (auto it = state->server->busted_target_to_target.find(target); it != state->server->busted_target_to_target.end()) {
		target = it->second;
		return_headers.emplace("Cache-Control", "max-age=7750000, immutable");
	}
	// If path leads to target of .GENERATED file, add the filename extension
	else if (auto it = state->server->files_generated_from_templates.find(target); it != state->server->files_generated_from_templates.end()) {
		target = FuzeHttp::insertExtensionToFileName(*it, ".GENERATED");
		return_headers.emplace("Cache-Control", "no-cache");
	}
	std::println("[showMainPage] will serve {}", target);
	std::string etag;
	if (auto it = state->server->manifest_frontend_etags.find(target); it != state->server->manifest_frontend_etags.end())
		etag = it->second;
	else
		etag = state->server->frontend_etag;

	if (auto if_none_match_header = req.find("If-None-Match"); if_none_match_header != req.end()) {
		std::string if_none_match_header_value = if_none_match_header->value();
		if (etag == if_none_match_header_value) {
			return {.status=http::status::not_modified};
		}
	}
	return_headers.emplace("ETag", etag);


	return {
		.status = http::status::ok,
		.headers = {return_headers},
		.file = std::format("{}/{}", state->getDocumentRoot().string(), target) // TODO change this because it sucks
	};
}

FuzeHttp::Response createGroup(shared_state* state, FuzeHttp::Request req) {
	std::optional<Client> client = state->getClientIfExists(req);
	std::print("Client rank: {}", state->getClientRank(client));
	std::print("Ordered_groups: {}", state->getOrderedGroups()->size());
	if (!state->clientHasPermission(client, PERMISSION::MANAGE_PERMISSIONS))
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
		std::cerr << "JSON error " << e.what() << std::endl;
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = std::format("[createGroup] {}", e.what())};
	}
	int new_group_rank = state->getClientRank(client) + 1;
	/*int new_group_id = */state->addGroup(new_group_name, new_group_rank);
	return FuzeHttp::Response{
		.status = http::status::created
	};
}

FuzeHttp::Response deleteGroup(shared_state* state, FuzeHttp::Request req, int group_id) {
	std::optional<Client> client = state->getClientIfExists(req);
	if (!state->groupExists(group_id))
		return FuzeHttp::Response{.status = http::status::not_found, .error_message = "Group does not exist."};
	if (!state->clientHasPermissionForGroup(client, PERMISSION::MANAGE_PERMISSIONS, group_id))
		return FuzeHttp::Response{.status = http::status::forbidden, .error_message = "You lack permission to delete this group."};
	state->eraseGroup(group_id);
	return FuzeHttp::Response{
		.status = http::status::ok
	};
}

FuzeHttp::Response removeMemberFromGroup(shared_state* state, FuzeHttp::Request req, int group_id, int account_id) {
	std::optional<Client> client = state->getClientIfExists(req);
	if (!state->groupExists(group_id))
		return FuzeHttp::Response{.status = http::status::not_found, .error_message = "Group does not exist."};
	if (!state->clientHasPermissionForGroup(client, PERMISSION::MANAGE_PERMISSIONS, group_id))
		return FuzeHttp::Response{.status = http::status::forbidden, .error_message = "You lack permission to delete this group."};
	if (!state->accountExists(account_id))
		return FuzeHttp::Response{.status = http::status::not_found, .error_message = std::format("Account {} not found.", account_id)};
	if (!state->clientHasPermissionForAccount(client, PERMISSION::MANAGE_PERMISSIONS, account_id))
		return FuzeHttp::Response{.status = http::status::forbidden, .error_message = "You lack permission to remove this account from a group."};
	state->removeUserFromGroup(account_id, group_id);
	return FuzeHttp::Response{
		.status = http::status::ok
	};
}

FuzeHttp::Response getGroupMembers(shared_state* state, FuzeHttp::Request req, int group_id) {
	if (!state->groupExists(group_id))
		return FuzeHttp::Response{.status = http::status::not_found, .error_message = "Group not found."};
	return FuzeHttp::Response{
		.status = http::status::ok,
		.json = state->getGroupMembersAsJson(group_id)
	};
}

FuzeHttp::Response getGroups(shared_state* state, FuzeHttp::Request req) {
	std::optional<Client> client = state->getClientIfExists(req);
	return FuzeHttp::Response{
		.status = http::status::ok,
		.body = state->dumpAllGroups(client)
	};
}

FuzeHttp::Response setGroupHeirarchy(shared_state* state, FuzeHttp::Request req) {
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
		std::cerr << "JSON error " << e.what() << std::endl;
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = std::format("[setGroupHeirarchy] {}", e.what())};
	}
	if (!state->clientHasPermission(client, PERMISSION::MANAGE_PERMISSIONS))
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

FuzeHttp::Response createThread(shared_state* state, FuzeHttp::Request req, Client client) {
	boost::json::object thread_json;
	try {
		thread_json = boost::json::parse(req.body()).at("thread").as_object();
	}
	catch(const std::exception& e) {
		std::cerr << "JSON error " << e.what() << std::endl;
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = std::format("[createThread] {}", e.what())};
	}
	if (!state->clientHasPermission(client, PERMISSION::CREATE_THREAD)) {
		return FuzeHttp::Response{
			.status = http::status::forbidden,
			.error_message = std::string("Client lacks permission CREATE_THREAD.")
		};
	}
	int new_thread_id = state->createThread(0, thread_json, client.id);

	return FuzeHttp::Response{
		.status = http::status::created,
		.headers = {{
			{"New-Thread-Id", std::to_string(new_thread_id)}
			// ,{"Location", std::format("/thread/{}/", new_thread_id)}
		}}
	};
}

FuzeHttp::Response createMessage(shared_state* state, FuzeHttp::Request req, Client client) {
	boost::json::object message_json;
	int thread_id;
	try {
		message_json = boost::json::parse(req.body()).at("post").as_object();
		thread_id = message_json.at("thread_id").as_int64();
	}
	catch(const std::exception& e) {
		std::cerr << "JSON error " << e.what() << std::endl;
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = std::format("[createMessage] {}", e.what())};
	}
	if (!state->main_board()->threadExists(thread_id))
		return FuzeHttp::Response{.status = http::status::not_found, .error_message = "This thread was not found."};
	if (!state->main_board()->getThread(thread_id)->clientHasPermission(client, PERMISSION::SEND_MESSAGE))
		return FuzeHttp::Response{.status = http::status::forbidden, .error_message = "User lacks permission SEND_MESSAGE within this thread"};
	int new_message_id = state->createMessage(0, message_json, client.id);
	std::string new_message_dump = state->main_board()->dumpMessage(thread_id, new_message_id);
	state->sendToThread(new_message_dump, thread_id);
	return FuzeHttp::Response{
		.status = http::status::created
	};
}

FuzeHttp::Response deletePost(shared_state* state, FuzeHttp::Request req, Client client, int thread_id, int message_id_in_thread) {
	if (!state->main_board()->threadExists(thread_id))
		return FuzeHttp::Response{.status = http::status::not_found, .error_message = "This thread was not found."};
	const Thread* thread = state->getThread(0, thread_id);
	if (!thread->messageExists(message_id_in_thread))
		return FuzeHttp::Response{.status = http::status::not_found, .error_message = "No such message found in this thread."};
	if (!thread->clientHasPermission(client, PERMISSION::DELETE_POST) && !thread->getMessage(message_id_in_thread)->clientIsAuthor(client))
		return FuzeHttp::Response{.status = http::status::forbidden, .error_message = "You lack permission to delete this message."};
	if (message_id_in_thread == 0)
		state->main_board()->deleteThread(thread_id);
	else
		state->main_board()->deleteMessageFromThread(message_id_in_thread, thread_id);
	return FuzeHttp::Response{
		.status = http::status::ok
	};
}

FuzeHttp::Response getThread(shared_state* state, FuzeHttp::Request req, int thread_id) {
	std::optional<Client> client = state->getClientIfExists(req);
	if (!state->main_board()->threadExists(thread_id))
		return FuzeHttp::Response{.status = http::status::not_found, .error_message = "This thread was not found."};
	const Thread* thread = state->main_board()->getThread(thread_id);
	if (thread->isDeleted())
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = "This thread has been deleted"};
	if (!thread->clientHasPermission(client, PERMISSION::VIEW_THREAD))
		return FuzeHttp::Response{.status = http::status::forbidden, .error_message = "You lack permission to view this thread."};
	return FuzeHttp::Response{
		.status = http::status::ok,
		.json = state->main_board()->getThread(thread_id)->asJsonWithMessages(client)
	};
}

FuzeHttp::Response getThreadPermissions(shared_state* state, FuzeHttp::Request req, int thread_id) {
	std::optional<Client> client = state->getClientIfExists(req);
	if (!state->main_board()->threadExists(thread_id))
		return FuzeHttp::Response{.status = http::status::not_found, .error_message = "This thread was not found."};
	else if (!state->main_board()->getThread(thread_id)->clientHasPermission(client, PERMISSION::VIEW_THREAD))
		return FuzeHttp::Response{.status = http::status::forbidden, .error_message = "You lack permission to view this thread."};
	return FuzeHttp::Response{
		.status = http::status::ok,
		.json = state->main_board()->getThread(thread_id)->getPermissionCollectionsAsJson()
	};
}

FuzeHttp::Response addThreadGroupPermission(shared_state* state, FuzeHttp::Request req, int thread_id, int group_id) {
	std::optional<Client> client = state->getClientIfExists(req);
	const Thread* thread = state->getThread(0, thread_id);
	if (!thread->clientHasPermissionForGroup(client, PERMISSION::MANAGE_PERMISSIONS, group_id))
		return FuzeHttp::Response{.status = http::status::forbidden, .error_message = "You lack permission manage permissions for this thread."};
	else if (thread->permissionCollectionExistsForGroup(group_id))
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = "Permissions for this group are already set. Use PUT request instead."};
	state->main_board()->addGroupPermissionCollectionToThread(group_id, thread_id);
	return FuzeHttp::Response{
		.status = http::status::created
	};
}

FuzeHttp::Response addThreadUserPermission(shared_state* state, FuzeHttp::Request req, int thread_id, int account_id) {
	std::optional<Client> client = state->getClientIfExists(req);
	const Thread* thread = state->getThread(0, thread_id);
	if (!thread->clientHasPermissionForAccount(client, PERMISSION::MANAGE_PERMISSIONS, account_id))
		return FuzeHttp::Response{.status = http::status::forbidden, .error_message = "You lack permission manage permissions for this thread."};
	else if (thread->permissionCollectionExistsForAccount(account_id))
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = "Permissions for this account are already set. Use PUT request instead."};
	state->main_board()->addAccountPermissionCollectionToThread(account_id, thread_id);
	return FuzeHttp::Response{
		.status = http::status::created
	};
}

FuzeHttp::Response updateThreadGroupPermissions(shared_state* state, FuzeHttp::Request req, int thread_id, int group_id) {
	std::optional<Client> client = state->getClientIfExists(req);
	const Thread* thread = state->getThread(0, thread_id);
	boost::json::object request_json;
	int permission_number, permission_setting;
	try {
		request_json = boost::json::parse(req.body()).as_object();
		permission_number = request_json["permission"].as_int64();
		permission_setting = request_json["setting"].as_int64();
	}
	catch(const std::exception& e) {
		std::cerr << "JSON error " << e.what() << std::endl;
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = std::format("[updateThreadGroupPermissions] {}", e.what())};
	}
	if (permission_number < 0 || permission_number >= static_cast<int>(PERMISSION::NUMBER_OF_PERMISSIONS))
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = "Invalid permission number in JSON"};
	if (permission_setting < 0 || permission_setting >= 3)
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = "Invalid permission setting in JSON"};
	if (!thread->clientHasPermissionForGroup(client, PERMISSION::MANAGE_PERMISSIONS, group_id))
		return FuzeHttp::Response{.status = http::status::forbidden, .error_message = "You lack permission to update permissions for this group."};
	state->main_board()->setGroupPermissionForThread(group_id, static_cast<PERMISSION>(permission_number), static_cast<THREE_STATE_SETTING>(permission_setting), thread_id);
	return FuzeHttp::Response{
		.status = http::status::ok
	};
}

FuzeHttp::Response updateThreadUserPermissions(shared_state* state, FuzeHttp::Request req, int thread_id, int account_id) {
	std::optional<Client> client = state->getClientIfExists(req);
	const Thread* thread = state->getThread(0, thread_id);
	boost::json::object request_json;
	int permission_number, permission_setting;
	try {
		request_json = boost::json::parse(req.body()).as_object();
		permission_number = request_json["permission"].as_int64();
		permission_setting = request_json["setting"].as_int64();
	}
	catch(const std::exception& e) {
		std::cerr << "JSON error " << e.what() << std::endl;
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = std::format("[updateThreadUserPermissions] {}", e.what())};
	}
	if (permission_number < 0 || permission_number >= static_cast<int>(PERMISSION::NUMBER_OF_PERMISSIONS))
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = "Invalid permission number in JSON"};
	if (permission_setting < 0 || permission_setting >= 3)
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = "Invalid permission setting in JSON"};
	if (!thread->clientHasPermissionForAccount(client, PERMISSION::MANAGE_PERMISSIONS, account_id))
		return FuzeHttp::Response{.status = http::status::forbidden, .error_message = "You lack permission to update permissions for this account."};
	state->main_board()->setAccountPermissionForThread(account_id, static_cast<PERMISSION>(permission_number), static_cast<THREE_STATE_SETTING>(permission_setting), thread_id);
	return FuzeHttp::Response{
		.status = http::status::ok
	};
}

FuzeHttp::Response deleteThreadGroupPermission(shared_state* state, FuzeHttp::Request req, int thread_id, int group_id) {		std::optional<Client> client = state->getClientIfExists(req);
	if (!state->main_board()->threadExists(thread_id))
		return FuzeHttp::Response{.status = http::status::not_found, .error_message = "This thread was not found."};
	const Thread* thread = state->getThread(0, thread_id);
	if (!thread->permissionCollectionExistsForGroup(group_id))
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = "No permissions set for this group."};
	if (!thread->clientHasPermissionForGroup(client, PERMISSION::MANAGE_PERMISSIONS, group_id))
		return FuzeHttp::Response{.status = http::status::forbidden, .error_message = "You lack permission to manage permissions for this group."};
	state->main_board()->removeGroupPermissionCollectionFromThread(group_id, thread_id);
	return FuzeHttp::Response{
		.status = http::status::ok
	};
}

FuzeHttp::Response deleteThreadUserPermission(shared_state* state, FuzeHttp::Request req, int thread_id, int account_id) {		std::optional<Client> client = state->getClientIfExists(req);
	if (!state->main_board()->threadExists(thread_id))
		return FuzeHttp::Response{.status = http::status::not_found, .error_message = "This thread was not found."};
	const Thread* thread = state->getThread(0, thread_id);
	if (!thread->permissionCollectionExistsForAccount(account_id))
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = "No permissions set for this account."};
	if (!thread->clientHasPermissionForAccount(client, PERMISSION::MANAGE_PERMISSIONS, account_id))
		return FuzeHttp::Response{.status = http::status::forbidden, .error_message = "You lack permission to manage permissions for this account."};
	state->main_board()->removeAccountPermissionCollectionFromThread(account_id, thread_id);
	return FuzeHttp::Response{
		.status = http::status::ok
	};
}

FuzeHttp::Response getThreads(shared_state* state, FuzeHttp::Request req) {
	std::optional<Client> client = state->getClientIfExists(req);
	std::println("called getThreads");
	return FuzeHttp::Response{
		.status = http::status::ok,
		.body = state->main_board()->dumpAllThreads(client)
	};
}

FuzeHttp::Response addGroupsToUser(shared_state* state, FuzeHttp::Request req, int account_id) {
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
		std::cerr << "JSON error " << e.what() << std::endl;
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = std::format("[createMessage] {}", e.what())};
	}
	if (!state->clientHasPermission(client, PERMISSION::MANAGE_PERMISSIONS))
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

FuzeHttp::Response getServerPermissions(shared_state* state, FuzeHttp::Request req) {
	return FuzeHttp::Response{
		.status = http::status::ok,
		.json = state->getPermissionCollectionsAsJson()
	};
}

FuzeHttp::Response addServerGroupPermission(shared_state* state, FuzeHttp::Request req, int group_id) {
	std::optional<Client> client = state->getClientIfExists(req);
	if (!state->clientHasPermissionForGroup(client, PERMISSION::MANAGE_PERMISSIONS, group_id))
		return FuzeHttp::Response{.status = http::status::forbidden, .error_message = "You lack permission to manage permissions."};
	else if (state->permissionCollectionExistsForGroup(group_id))
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = "Permissions for this group are already set. Use PUT request instead."};
	state->addGroupPermissionCollection(group_id);
	return FuzeHttp::Response{
		.status = http::status::created
	};
}

FuzeHttp::Response addServerUserPermission(shared_state* state, FuzeHttp::Request req, int account_id) {
	std::optional<Client> client = state->getClientIfExists(req);
	if (!state->clientHasPermissionForAccount(client, PERMISSION::MANAGE_PERMISSIONS, account_id))
		return FuzeHttp::Response{.status = http::status::forbidden, .error_message = "You lack permission to manage permissions."};
	else if (state->permissionCollectionExistsForAccount(account_id))
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = "Permissions for this account are already set. Use PUT request instead."};
	state->addAccountPermissionCollection(account_id);
	return FuzeHttp::Response{
		.status = http::status::created
	};
}

FuzeHttp::Response updateServerGroupPermissions(shared_state* state, FuzeHttp::Request req, int group_id) {
	std::optional<Client> client = state->getClientIfExists(req);
	boost::json::object request_json;
	int permission_number, permission_setting;
	try {
		request_json = boost::json::parse(req.body()).as_object();
		permission_number = request_json["permission"].as_int64();
		permission_setting = request_json["setting"].as_int64();
	}
	catch(const std::exception& e) {
		std::cerr << "JSON error " << e.what() << std::endl;
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = std::format("[updateServerGroupPermissions] {}", e.what())};
	}
	if (permission_number < 0 || permission_number >= static_cast<int>(PERMISSION::NUMBER_OF_PERMISSIONS))
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = "Invalid permission number in JSON"};
	if (permission_setting < 0 || permission_setting >= 3)
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = "Invalid permission setting in JSON"};
	if (!state->clientHasPermissionForGroup(client, PERMISSION::MANAGE_PERMISSIONS, group_id))
		return FuzeHttp::Response{.status = http::status::forbidden, .error_message = "You lack permission to update permissions for this group."};
	state->setGroupPermission(group_id, static_cast<PERMISSION>(permission_number), static_cast<THREE_STATE_SETTING>(permission_setting));
	return FuzeHttp::Response{
		.status = http::status::created
	};
}

FuzeHttp::Response updateServerUserPermissions(shared_state* state, FuzeHttp::Request req, int account_id) {
	std::optional<Client> client = state->getClientIfExists(req);
	boost::json::object request_json;
	int permission_number, permission_setting;
	try {
		request_json = boost::json::parse(req.body()).as_object();
		permission_number = request_json["permission"].as_int64();
		permission_setting = request_json["setting"].as_int64();
	}
	catch(const std::exception& e) {
		std::cerr << "JSON error " << e.what() << std::endl;
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = std::format("[updateServerUserPermissions] {}", e.what())};
	}
	if (permission_number < 0 || permission_number >= static_cast<int>(PERMISSION::NUMBER_OF_PERMISSIONS))
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = "Invalid permission number in JSON"};
	if (permission_setting < 0 || permission_setting >= 3)
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = "Invalid permission setting in JSON"};
	if (!state->clientHasPermissionForAccount(client, PERMISSION::MANAGE_PERMISSIONS, account_id))
		return FuzeHttp::Response{.status = http::status::forbidden, .error_message = "You lack permission to update permissions for this account."};
	state->setAccountPermission(account_id, static_cast<PERMISSION>(permission_number), static_cast<THREE_STATE_SETTING>(permission_setting));
	return FuzeHttp::Response{
		.status = http::status::created
	};
}

FuzeHttp::Response deleteServerGroupPermission(shared_state* state, FuzeHttp::Request req, int group_id) {
	std::optional<Client> client = state->getClientIfExists(req);
	if (!state->permissionCollectionExistsForGroup(group_id))
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = "This group does not exist."};
	if (!state->clientHasPermissionForGroup(client, PERMISSION::MANAGE_PERMISSIONS, group_id))
		return FuzeHttp::Response{.status = http::status::forbidden, .error_message = "You lack permission to delete this group."};
	state->removeGroupPermissionCollection(group_id);
	return FuzeHttp::Response{
		.status = http::status::ok
	};
}

FuzeHttp::Response deleteServerUserPermission(shared_state* state, FuzeHttp::Request req, int account_id) {
	std::optional<Client> client = state->getClientIfExists(req);
	if (!state->permissionCollectionExistsForAccount(account_id))
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = "This account does not exist."};
	if (!state->clientHasPermissionForAccount(client, PERMISSION::MANAGE_PERMISSIONS, account_id))
		return FuzeHttp::Response{.status = http::status::forbidden, .error_message = "You lack permission to delete this account."};
	state->removeAccountPermissionCollection(account_id);
	return FuzeHttp::Response{
		.status = http::status::ok
	};
}

FuzeHttp::Response client(shared_state* state, FuzeHttp::Request req) {
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
				{"manage_permissions", state->clientHasPermission(client, PERMISSION::MANAGE_PERMISSIONS)},
				{"create_thread", state->clientHasPermission(client, PERMISSION::CREATE_THREAD)},
				{"upload_file", state->clientHasPermission(client, PERMISSION::UPLOAD_FILE)}
			}},
			{"has_cookie", req.find("Cookie") != req.end()}
		}}
	};
}

FuzeHttp::Response getUsers(shared_state* state, FuzeHttp::Request req) {
	std::optional<Client> client = state->getClientIfExists(req);
	return FuzeHttp::Response{
		.status = http::status::ok,
		.headers = {{
			{"Client-Rank", std::to_string(state->getClientRank(client))}
		}},
		.body = state->dumpAllUsers(client)
	};
}

FuzeHttp::Response acceptInvite(shared_state* state, FuzeHttp::Request req, std::string invite_key_base64) {
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
