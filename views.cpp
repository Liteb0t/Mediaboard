#include "views.hpp"
#include "FuzeHttp.hpp"
#include "permission_managed_object.hpp"
#include "shared_state.hpp"
#include <boost/beast/http/status.hpp>
#include <iostream>

FuzeHttp::Response showMainPage(shared_state* state, FuzeHttp::Request req) {
	return FuzeHttp::Response{
		.status = http::status::ok,
		.file = std::format("{}/frontend/index.html", state->getProgramLocation().string())
	};
}

FuzeHttp::Response createThread(shared_state* state, FuzeHttp::Request req, FuzeHttp::Client client) {
	boost::json::object thread_json;
	try {
		thread_json = boost::json::parse(req.body()).as_object();
	}
	catch(const std::exception& e) {
		std::cerr << "JSON error" << std::endl;
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = std::format("[createThread] {}", e.what())};
	}
	std::cout << "Client ID " << client.id << std::endl;
	if (client.account_id) {
		std::cout << "ACCOUNT_ID FOUND ";
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

FuzeHttp::Response getThreads(shared_state* state, FuzeHttp::Request req) {
	std::optional<FuzeHttp::Client> client = state->getClientIfExists(req);
	return FuzeHttp::Response{
		.status = http::status::ok,
		.body = state->main_board()->dumpAllThreads(client)
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
		.file = std::format("{}/frontend/index.html", state->getProgramLocation().string())
	};
}
