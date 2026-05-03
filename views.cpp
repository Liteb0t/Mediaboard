// FUZE.page 2026
// The following code is not to be used for AI training. For humans, the MIT license applies.
#include "views.hpp"
#include "FuzeHttp.hpp"
#include "permission_managed_object.hpp"
#include "shared_state.hpp"
#include <boost/beast/http/status.hpp>
#include <iostream>

FuzeHttp::Response showMainPage(shared_state* state, FuzeHttp::Request req) {
	return FuzeHttp::Response{
		.status = http::status::ok,
		.file = std::format("{}/index.html", state->getDocumentRoot().string())
	};
}

FuzeHttp::Response createGroup(shared_state* state, FuzeHttp::Request req) {
	std::optional<FuzeHttp::Client> client = state->getClientIfExists(req);
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

FuzeHttp::Response getGroupMembers(shared_state* state, FuzeHttp::Request req, int group_id) {
	if (!state->groupExists(group_id))
		return FuzeHttp::Response{.status = http::status::not_found, .error_message = "Group not found."};
	return FuzeHttp::Response{
		.status = http::status::ok,
		.json = state->getGroupMembersAsJson(group_id)
	};
}

FuzeHttp::Response getGroups(shared_state* state, FuzeHttp::Request req) {
	std::optional<FuzeHttp::Client> client = state->getClientIfExists(req);
	return FuzeHttp::Response{
		.status = http::status::ok,
		.body = state->dumpAllGroups(client)
	};
}

FuzeHttp::Response createThread(shared_state* state, FuzeHttp::Request req, FuzeHttp::Client client) {
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
	int new_thread_id = state->main_board()->createThread(thread_json, client.id);

	return FuzeHttp::Response{
		.status = http::status::created,
		.headers = {{
			{"New-Thread-Id", std::to_string(new_thread_id)}
			// ,{"Location", std::format("/thread/{}/", new_thread_id)}
		}}
	};
}

FuzeHttp::Response createMessage(shared_state* state, FuzeHttp::Request req, FuzeHttp::Client client) {
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
	int new_message_id = state->main_board()->createMessage(message_json, client.id);
	std::string new_message_dump = state->main_board()->dumpMessage(thread_id, new_message_id);
	state->sendToThread(new_message_dump, thread_id);
	return FuzeHttp::Response{
		.status = http::status::created
	};
}

FuzeHttp::Response getThread(shared_state* state, FuzeHttp::Request req, int thread_id) {
	std::optional<FuzeHttp::Client> client = state->getClientIfExists(req);
	if (!state->main_board()->threadExists(thread_id))
		return FuzeHttp::Response{.status = http::status::not_found, .error_message = "This thread was not found."};
	else if (!state->main_board()->getThread(thread_id)->clientHasPermission(client, PERMISSION::VIEW_THREAD))
		return FuzeHttp::Response{.status = http::status::forbidden, .error_message = "You lack permission to view this thread."};
	return FuzeHttp::Response{
		.status = http::status::ok,
		.json = state->main_board()->getThread(thread_id)->asJsonWithMessages(client)
	};
}

FuzeHttp::Response getThreadPermissions(shared_state* state, FuzeHttp::Request req, int thread_id) {
	std::optional<FuzeHttp::Client> client = state->getClientIfExists(req);
	if (!state->main_board()->threadExists(thread_id))
		return FuzeHttp::Response{.status = http::status::not_found, .error_message = "This thread was not found."};
	else if (!state->main_board()->getThread(thread_id)->clientHasPermission(client, PERMISSION::VIEW_THREAD))
		return FuzeHttp::Response{.status = http::status::forbidden, .error_message = "You lack permission to view this thread."};
	return FuzeHttp::Response{
		.status = http::status::ok,
		.json = state->main_board()->getThreadPermissionsAsJson(thread_id, client)
	};
}

FuzeHttp::Response setThreadGroupPermissions(shared_state* state, FuzeHttp::Request req, int thread_id, int group_id) {
	std::optional<FuzeHttp::Client> client = state->getClientIfExists(req);
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

FuzeHttp::Response setThreadUserPermissions(shared_state* state, FuzeHttp::Request req, int thread_id, int account_id) {
	std::optional<FuzeHttp::Client> client = state->getClientIfExists(req);
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

FuzeHttp::Response getThreads(shared_state* state, FuzeHttp::Request req) {
	std::optional<FuzeHttp::Client> client = state->getClientIfExists(req);
	return FuzeHttp::Response{
		.status = http::status::ok,
		.body = state->main_board()->dumpAllThreads(client)
	};
}

FuzeHttp::Response getServerPermissions(shared_state* state, FuzeHttp::Request req) {
	return FuzeHttp::Response{
		.status = http::status::ok,
		.json = state->getPermissionCollectionsAsJson()
	};
}

FuzeHttp::Response client(shared_state* state, FuzeHttp::Request req) {
	std::optional<FuzeHttp::Client> client = state->getClientIfExists(req);
	if (client) {
		std::cout << "CLIENT FOUND ";
		if (client.value().account_id)
			std::cout << "ACCOUNT_ID FOUND ";
	}
	return FuzeHttp::Response{
		.status = http::status::ok,
		.json = {{
			{"server_permissions", {
				{"manage_permissions", state->clientHasPermission(client, PERMISSION::MANAGE_PERMISSIONS)},
				{"create_thread", state->clientHasPermission(client, PERMISSION::CREATE_THREAD)}
			}}
		}}
	};
}

FuzeHttp::Response getUsers(shared_state* state, FuzeHttp::Request req) {
	std::optional<FuzeHttp::Client> client = state->getClientIfExists(req);
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

// TODO find a way to handle multiple directories under one view
FuzeHttp::Response getMedia(shared_state* state, FuzeHttp::Request req, std::string file_path) {
	std::cout << "Showing thru getMedia" << std::endl;
	std::string file_name = file_path;
	int filename_extension_index;
	if ((filename_extension_index = file_name.rfind(".")) == -1) {
		filename_extension_index = file_name.size();
	}
	if (filename_extension_index >= 36) {
		file_name.erase(filename_extension_index - 36, 36);
	}
	return FuzeHttp::Response{
		.status = http::status::ok,
		.headers = {{
			{"Content-Disposition", std::format("inline; filename=\"{}\"", file_name)}
		}},
		.file = std::format("{}/{}", state->getMediaLocation().string(), file_path)
	};
}

FuzeHttp::Response getThumbnail(shared_state* state, FuzeHttp::Request req, std::string file_path) {
	std::cout << "Showing thru getMedia" << std::endl;
	std::string file_name = file_path;
	int filename_extension_index;
	if ((filename_extension_index = file_name.rfind(".")) == -1) {
		filename_extension_index = file_name.size();
	}
	if (filename_extension_index >= 36) {
		file_name.erase(filename_extension_index - 36, 36);
	}
	return FuzeHttp::Response{
		.status = http::status::ok,
		.file = std::format("{}/thumbnails/{}", state->getMediaLocation().string(), file_path)
	};
}

FuzeHttp::Response showThread(shared_state* state, FuzeHttp::Request req, int thread_id) {
	return FuzeHttp::Response{
		.status = http::status::ok,
		.file = std::format("{}/index.html", state->getDocumentRoot().string())
	};
}
