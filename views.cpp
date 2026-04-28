#include "views.hpp"
#include "FuzeHttp.hpp"
#include "permission_managed_object.hpp"
#include "shared_state.hpp"
#include "sodium/crypto_generichash.h"
#include <boost/beast/http/status.hpp>
#include <iostream>

FuzeHttp::Response testView(shared_state* state, FuzeHttp::Request req, std::string var) {
	FuzeHttp::Response res;
	std::cout << "Called testview with var = " << var << std::endl;
	res.status = http::status::i_am_a_teapot;
	boost::json::object json;
	json["test"] = 73;
	json["req.target()"] = req.target();
	res.json = std::move(json);
	return res;
}

FuzeHttp::Response requestNewAccountParameters(shared_state* state, FuzeHttp::Request req) {
	boost::json::object req_json;
	boost::json::string username_j;
	std::optional<std::string> invite_key;
	try {
		req_json = boost::json::parse(req.body()).as_object();
		username_j = req_json.at("username").as_string();
		if (boost::json::object::const_iterator invite_key_it = req_json.find("invite"); invite_key_it != req_json.end())
			invite_key = req_json.at("invite").as_string().c_str();
	}
	catch(const std::exception& e) {
		return FuzeHttp::Response{.status = http::status::internal_server_error, .error_message = std::format("[registerAccount] {}", e.what())};
	}
	std::string username = std::string(username_j);
	if (username.length() > ACCOUNT_MAX_USERNAME)
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = std::format("Username length {} is over the limit of {}", username.length(), ACCOUNT_MAX_USERNAME)};
	else if (username.empty())
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = std::string("Username cannot be empty")};
	// TODO check for bad characters in username
	// Note: closed registration is required for resistance to account enumeration attacks.
	if (invite_key) {
		int granted_group = state->getGrantedGroupIdFromInvite(invite_key.value());
		if (granted_group == static_cast<int>(BUILTIN_GROUPS::PUBLIC)) {
			return FuzeHttp::Response{
				.status = http::status::bad_request,
				.body = "This invite link is invalid. It may have expired, or it might never had existed to begin with."
			};
		}
	}

	unsigned char intermediate_salt[crypto_pwhash_SALTBYTES];
	randombytes_buf(intermediate_salt, crypto_pwhash_SALTBYTES);
	char intermediate_salt_base64[sodium_base64_ENCODED_LEN(crypto_pwhash_SALTBYTES, sodium_base64_VARIANT_URLSAFE)];
	sodium_bin2base64(
		intermediate_salt_base64, sizeof intermediate_salt_base64,
		intermediate_salt, crypto_pwhash_SALTBYTES,
		sodium_base64_VARIANT_URLSAFE
	);
	std::string intermediate_salt_base64_str = intermediate_salt_base64;
	std::cout << "intermediate_salt_base64: " << intermediate_salt_base64_str << std::endl;
	unsigned char salt[crypto_pwhash_SALTBYTES];
	crypto_generichash(
		salt, crypto_pwhash_SALTBYTES,
		reinterpret_cast<const unsigned char*>(username.c_str()), username.length(),
		reinterpret_cast<const unsigned char*>(intermediate_salt_base64_str.c_str()), intermediate_salt_base64_str.length()
	);
	char salt_base64[sodium_base64_ENCODED_LEN(crypto_pwhash_SALTBYTES, sodium_base64_VARIANT_URLSAFE)];
	sodium_bin2base64(
		salt_base64, sodium_base64_ENCODED_LEN(crypto_pwhash_SALTBYTES, sodium_base64_VARIANT_URLSAFE),
		salt, crypto_pwhash_SALTBYTES,
		sodium_base64_VARIANT_URLSAFE
	);
	return FuzeHttp::Response{
		.status = http::status::ok,
		.json = {{
			{ "intermediate_salt_base64", intermediate_salt_base64 },
			{ "salt_base64", salt_base64 },
			{ "password_hash_length", crypto_pwhash_STRBYTES },
			{ "pwhash_opslimit", state->client_pwhash_opslimit },
			{ "pwhash_memlimit", state->client_pwhash_memlimit }
		}}
	};
}

FuzeHttp::Response createNewAccount(shared_state* state, FuzeHttp::Request req) {
	boost::json::object req_json;
	boost::json::string username_j, password_hash_base64, intermediate_salt_base64;
	std::optional<std::string> invite_key;
	std::optional<int> invite_granted_group_id;
	std::cout << "createNewAccount called" << std::endl;
	try {
		req_json = boost::json::parse(req.body()).as_object();
		username_j = req_json.at("username").as_string();
		intermediate_salt_base64 = req_json.at("intermediate_salt_base64").as_string();
		password_hash_base64 = req_json.at("password_hash_base64").as_string();
		if (boost::json::object::const_iterator invite_key_it = req_json.find("invite"); invite_key_it != req_json.end())
			invite_key = req_json.at("invite").as_string().c_str();
	}
	catch(const std::exception& e) {
		std::cout << "JSON error" << std::endl;
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = std::format("[registerAccount] {}", e.what())};
	}
	std::string username = std::string(username_j);
	if (username.length() > ACCOUNT_MAX_USERNAME)
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = std::format("Username length {} is over the limit of {}", username.length(), ACCOUNT_MAX_USERNAME)};
	else if (password_hash_base64.size() > 500)
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = std::format("password_hash_base64 length {} is over the limit of 500", password_hash_base64.size())};
	else if (username.empty())
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = std::string("Username cannot be empty")};
	else if (state->accountExists(username.data()))
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = std::string("There already exists an account with this username.")};

	if (invite_key) {
		invite_granted_group_id = state->getGrantedGroupIdFromInvite(invite_key.value());
		if (invite_granted_group_id == static_cast<int>(BUILTIN_GROUPS::PUBLIC)) {
			return FuzeHttp::Response{
				.status = http::status::bad_request,
				.body = "This invite link is invalid. It may have expired, or it might never had existed to begin with."
			};
		}
	}

	char password_hash_hash_base64[sodium_base64_ENCODED_LEN(crypto_generichash_BYTES, sodium_base64_VARIANT_URLSAFE)];
	FuzeHttp::generatePasswordHashHashBase64(
		password_hash_hash_base64, sizeof password_hash_hash_base64,
		password_hash_base64.c_str(), password_hash_base64.size()
	);

	int user_id;
	try {
		user_id = state->createAccount(username, std::move(password_hash_hash_base64), intermediate_salt_base64.c_str());
		if (invite_granted_group_id) {
			state->addUserToGroup(user_id, invite_granted_group_id.value());
			std::cout << "add user to group" << std::endl;
			//if (invite_granted_group_id.value() == static_cast<int>(BUILTIN_GROUPS::OWNER))
			//	state->db->setOwner(user_id);
		}
	}
	catch(const std::exception& e) {
		std::cout << "createAccount error" << std::endl;
		return FuzeHttp::Response{.status = http::status::internal_server_error, .error_message = std::format("[createNewAccount] {}", e.what())};
	}
	std::cout << "Created account " << username << std::endl;
	std::string session_id_base64 = state->createSession(user_id);
	return FuzeHttp::Response{
		.status = http::status::created,
		.headers = {{{"Set-Cookie", FuzeHttp::formatCookie(session_id_base64)}}}
	};
}

FuzeHttp::Response requestLoginParameters(shared_state* state, FuzeHttp::Request req) {
	boost::json::value req_json;
	boost::json::string username_j;
	try {
		req_json = boost::json::parse(req.body());
		username_j = req_json.at("username").as_string();
	}
	catch(const std::exception& e) {
		return FuzeHttp::Response{.status = http::status::internal_server_error, .error_message = std::format("[registerAccount] {}", e.what())};
	}
	std::string username = std::string(username_j);
	if (username.length() > ACCOUNT_MAX_USERNAME)
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = std::format("Username length {} is over the limit of {}", username.length(), ACCOUNT_MAX_USERNAME)};
	else if (username.empty())
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = std::string("Username cannot be empty")};

	// unsigned char intermediate_salt[crypto_pwhash_SALTBYTES];
	char salt_base64[sodium_base64_ENCODED_LEN(crypto_pwhash_SALTBYTES, sodium_base64_VARIANT_URLSAFE)];
	FuzeHttp::getSaltBase64(state, username, salt_base64);
	boost::json::object json = {
		{ "salt_base64", salt_base64 },
		{ "password_hash_length", crypto_pwhash_STRBYTES },
		{ "pwhash_opslimit", state->client_pwhash_opslimit },
		{ "pwhash_memlimit", state->client_pwhash_memlimit }
	};
	return FuzeHttp::Response{.status = http::status::ok, .json = std::move(json)};
}

FuzeHttp::Response login(shared_state* state, FuzeHttp::Request req) {
	boost::json::value req_json;
	boost::json::string username_j, password_hash_base64;
	std::cout << "createNewAccount called" << std::endl;
	try {
		req_json = boost::json::parse(req.body());
		username_j = req_json.at("username").as_string();
		password_hash_base64 = req_json.at("password_hash_base64").as_string();
	}
	catch(const std::exception& e) {
		std::cout << "JSON error" << std::endl;
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = std::format("[login] {}", e.what())};
	}
	std::string username = std::string(username_j);
	if (username.length() > ACCOUNT_MAX_USERNAME)
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = std::format("Username length {} is over the limit of {}", username.length(), ACCOUNT_MAX_USERNAME)};
	else if (username.empty())
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = std::string("Username cannot be empty")};

	char password_hash_hash_base64[sodium_base64_ENCODED_LEN(crypto_generichash_BYTES, sodium_base64_VARIANT_URLSAFE)];
	FuzeHttp::generatePasswordHashHashBase64(
		password_hash_hash_base64, sizeof password_hash_hash_base64,
		password_hash_base64.c_str(), password_hash_base64.size()
	);
	std::optional<int> account_id = state->getIdFromUsername(username);
	if (account_id && state->accountMatchesPassword(account_id.value(), password_hash_hash_base64)) {
		std::string session_id_base64;
		try {
			session_id_base64 = state->createSession(account_id.value()); // Add session so client can authenticate via browser cookie
		}
		catch(const std::exception& e) {
			std::string error_text = std::format("[login] {}", e.what());
			std::cout << error_text << std::endl;
			return FuzeHttp::Response{.status = http::status::bad_request, .error_message = error_text};
		}
		return FuzeHttp::Response{
			.status = http::status::accepted,
			.headers = FuzeHttp::Headers{{"Set-Cookie", FuzeHttp::formatCookie(session_id_base64)}}
		};
	}
	else {
		return FuzeHttp::Response{
			.status = http::status::unauthorized,
			.error_message = std::string("Password is incorrect or the user doesn't exist.")
		};
	}
}

FuzeHttp::Response threads(shared_state* state, FuzeHttp::Request req) {
	std::variant<FuzeHttp::Client, FuzeHttp::Response> client = state->getRequiredClient(req);
	if (client.index() != 0)
		return std::get<1>(client);
	// std::optional<FuzeHttp::Client> client = state->getClientIfExists(req);
	return FuzeHttp::Response{
		.status = http::status::ok,
		.body = state->main_board()->dumpAllThreads(std::get<0>(client))
	};
}

FuzeHttp::Response client(shared_state* state, FuzeHttp::Request req) {
	// std::variant<Client, FuzeHttp::Response> client = state->getRequiredClient(req);
	// if (client.index() != 0)
	// 	return std::get<1>(client);
	return FuzeHttp::Response{
		.status = http::status::ok,
		// .json = {{
		// 	{"server_permissions", {
		// 		{"manage_permissions", state->clientHasPermission(std::get<0>(client), PERMISSION::MANAGE_PERMISSIONS)},
		// 		{"create_thread", state->clientHasPermission(std::get<0>(client), PERMISSION::CREATE_THREAD)}
		// 	}}
		// }}
	};
}

FuzeHttp::Response acceptInvite(shared_state* state, FuzeHttp::Request req, FuzeHttp::Client client, std::string invite_key_base64) {
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
