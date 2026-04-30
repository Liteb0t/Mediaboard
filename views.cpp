#include "views.hpp"
#include "FuzeHttp.hpp"
#include "permission_managed_object.hpp"
#include "shared_state.hpp"
#include <boost/beast/http/status.hpp>
#include <iostream>

FuzeHttp::Response createThread(shared_state* state, FuzeHttp::Request req, FuzeHttp::Client client) {
	boost::json::object thread_json;
	try {
		thread_json = boost::json::parse(req.body()).as_object();
	}
	catch(const std::exception& e) {
		std::cerr << "JSON error" << std::endl;
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

FuzeHttp::Response threads(shared_state* state, FuzeHttp::Request req) {
	std::optional<FuzeHttp::Client> client = state->getClientIfExists(req);
	return FuzeHttp::Response{
		.status = http::status::ok,
		.body = state->main_board()->dumpAllThreads(client)
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
