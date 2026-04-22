#include "views.hpp"
#include "DatabaseConnection.hpp"
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
	// TODO check for bad characters in username
	// Note: closed registration is required for resistance to account enumeration attacks.
	unsigned char intermediate_salt[crypto_pwhash_SALTBYTES];
	randombytes_buf(intermediate_salt, crypto_pwhash_SALTBYTES);
	char intermediate_salt_base64[sodium_base64_ENCODED_LEN(crypto_pwhash_SALTBYTES, sodium_base64_VARIANT_URLSAFE)];
	sodium_bin2base64(
		intermediate_salt_base64, sizeof intermediate_salt_base64,
		intermediate_salt, crypto_pwhash_SALTBYTES,
		sodium_base64_VARIANT_URLSAFE
	);
	std::string intermediate_salt_base64_str = intermediate_salt_base64;
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
			{ "salt_base64", salt_base64 }
			// ,{ "pwhash_opslimit", state->client_pwhash_opslimit }
			// ,{ "pwhash_memlimit", state->client_pwhash_memlimit }
		}}
	};
}

FuzeHttp::Response createNewAccount(shared_state* state, FuzeHttp::Request req) {
	boost::json::value req_json;
	boost::json::string username_j, password_hash_base64, intermediate_salt_base64;
	std::cout << "createNewAccount called" << std::endl;
	try {
		req_json = boost::json::parse(req.body());
		username_j = req_json.at("username").as_string();
		intermediate_salt_base64 = req_json.at("intermediate_salt_base64").as_string();
		password_hash_base64 = req_json.at("password_hash_base64").as_string();
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
	else if (state->db->getAccountByUsername(username.data()))
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = std::string("There already exists an account with this username.")};

	char password_hash_hash_base64[sodium_base64_ENCODED_LEN(crypto_generichash_BYTES, sodium_base64_VARIANT_URLSAFE)];
	FuzeHttp::generatePasswordHashHashBase64(
		password_hash_hash_base64, sizeof password_hash_hash_base64,
		password_hash_base64.c_str(), password_hash_base64.size()
	);

	int user_id;
	try {
		user_id = state->db->createAccount(username, std::move(password_hash_hash_base64), intermediate_salt_base64.c_str());
	}
	catch(const std::exception& e) {
		std::cout << "createAccount error" << std::endl;
		return FuzeHttp::Response{.status = http::status::internal_server_error, .error_message = std::format("[createNewAccount] {}", e.what())};
	}
	std::cout << "Created account " << username << std::endl;
	unsigned char session_id_bytes[128/8];
	randombytes_buf(session_id_bytes, 128/8);
	char session_id_base64[sodium_base64_ENCODED_LEN(128/8, sodium_base64_VARIANT_URLSAFE)];
	sodium_bin2base64(session_id_base64, sodium_base64_ENCODED_LEN(128/8, sodium_base64_VARIANT_URLSAFE), session_id_bytes, 128/8, sodium_base64_VARIANT_URLSAFE);
	state->addSession(session_id_base64, {.user_id = user_id});
	return FuzeHttp::Response{
		.status = http::status::created,
		.headers = {{{"Set-Cookie", std::format("{}; HttpOnly", session_id_base64)}}}
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
		{ "salt_base64", salt_base64 }
		// ,{ "pwhash_opslimit", state->client_pwhash_opslimit }
		// ,{ "pwhash_memlimit", state->client_pwhash_memlimit }
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
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = std::format("[registerAccount] {}", e.what())};
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
	int user_id = state->db->getAccountByUsername(username);
	if (user_id != BUILTIN_USERS::PUBLIC) {
		if (state->db->userMatchesPassword(user_id, password_hash_hash_base64)) {
			// Add session so client can authenticate via browser cookie
			unsigned char session_id_bytes[128/8];
			randombytes_buf(session_id_bytes, 128/8);
			char session_id_base64[sodium_base64_ENCODED_LEN(128/8, sodium_base64_VARIANT_URLSAFE)];
			sodium_bin2base64(session_id_base64, sodium_base64_ENCODED_LEN(128/8, sodium_base64_VARIANT_URLSAFE), session_id_bytes, 128/8, sodium_base64_VARIANT_URLSAFE);
			// TODO add expiration
			state->addSession(session_id_base64, {
				.user_id = user_id
			});
			return FuzeHttp::Response{
				.status = http::status::accepted,
				.headers = FuzeHttp::Headers{{"Set-Cookie", std::format("{}; HttpOnly", session_id_base64)}}
			};
		}
	}
	return FuzeHttp::Response{
		.status = http::status::unauthorized,
		.error_message = std::string("Password is incorrect or the user doesn't exist.")
	};
}
