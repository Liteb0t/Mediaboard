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
	boost::json::string username;
	try {
		req_json = boost::json::parse(req.body());
		username = req_json.at("username").as_string();
	}
	catch(const std::exception& e) {
		return FuzeHttp::Response{.status = http::status::internal_server_error, .error_message = std::format("[registerAccount] {}", e.what())};
	}
	if (username.size() > ACCOUNT_MAX_USERNAME)
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = std::format("Username length {} is over the limit of {}", username.size(), ACCOUNT_MAX_USERNAME)};
	else if (username.empty())
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = std::string("Username cannot be empty")};
	// TODO check for bad characters in username
	// Note: closed registration is required for resistance to account enumeration attacks.
	unsigned char intermediate_salt[crypto_pwhash_SALTBYTES];
	unsigned char salt[crypto_pwhash_SALTBYTES];
	auto it = state->intermediate_account_registrations.find(username.c_str());
	if (it != state->intermediate_account_registrations.end())
		memcpy(intermediate_salt, it->second.value, crypto_pwhash_SALTBYTES);
	// else if username found in database, get the contini value
	else {
		randombytes_buf(intermediate_salt, crypto_pwhash_SALTBYTES);
		IntermediateSalt is;
		memcpy(is.value, intermediate_salt, crypto_pwhash_SALTBYTES);
		state->intermediate_account_registrations.emplace(std::string(username), std::move(is));
	}
	char intermediate_salt_base64[sodium_base64_ENCODED_LEN(crypto_pwhash_SALTBYTES, sodium_base64_VARIANT_URLSAFE)];
	sodium_bin2base64(intermediate_salt_base64, sodium_base64_ENCODED_LEN(crypto_pwhash_SALTBYTES, sodium_base64_VARIANT_URLSAFE), it->second.value, crypto_pwhash_SALTBYTES, sodium_base64_VARIANT_URLSAFE);
	crypto_generichash(salt, sizeof salt,
					reinterpret_cast<const unsigned char*>(username.c_str()), username.size(),
					   reinterpret_cast<const unsigned char*>(intermediate_salt_base64), sodium_base64_ENCODED_LEN(crypto_pwhash_SALTBYTES, sodium_base64_VARIANT_URLSAFE));
	char salt_base64[sodium_base64_ENCODED_LEN(crypto_pwhash_SALTBYTES, sodium_base64_VARIANT_URLSAFE)];
	sodium_bin2base64(salt_base64, sodium_base64_ENCODED_LEN(crypto_pwhash_SALTBYTES, sodium_base64_VARIANT_URLSAFE), salt, crypto_pwhash_SALTBYTES, sodium_base64_VARIANT_URLSAFE);
	boost::json::object json = {
		{ "salt_base64", salt_base64 }
		// ,{ "pwhash_opslimit", state->client_pwhash_opslimit }
		// ,{ "pwhash_memlimit", state->client_pwhash_memlimit }
	};
	return FuzeHttp::Response{.status = http::status::ok, .json = std::move(json)};
}

FuzeHttp::Response createNewAccount(shared_state* state, FuzeHttp::Request req) {
	boost::json::value req_json;
	boost::json::string username, password_hash_base64;
	std::cout << "createNewAccount called" << std::endl;
	try {
		req_json = boost::json::parse(req.body());
		username = req_json.at("username").as_string();
		password_hash_base64 = req_json.at("password_hash_base64").as_string();
	}
	catch(const std::exception& e) {
		std::cout << "JSON error" << std::endl;
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = std::format("[registerAccount] {}", e.what())};
	}
	if (username.size() > ACCOUNT_MAX_USERNAME)
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = std::format("Username length {} is over the limit of {}", username.size(), ACCOUNT_MAX_USERNAME)};
	else if (password_hash_base64.size() > 255)
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = std::format("password_hash_base64 length {} is over the limit of 255", password_hash_base64.size())};
	else if (username.empty())
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = std::string("Username cannot be empty")};
	else if (state->db->getAccountByUsername(username.data()))
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = std::string("There already exists an account with this username.")};

	// hash of password hash in base64 is stored in DB
	unsigned char password_hash_hash[crypto_generichash_BYTES];
	crypto_generichash(password_hash_hash, crypto_generichash_BYTES, reinterpret_cast<const unsigned char*>(password_hash_base64.c_str()), password_hash_base64.size(), NULL, 0);
	char password_hash_hash_base64[sodium_base64_ENCODED_LEN(crypto_generichash_BYTES, sodium_base64_VARIANT_URLSAFE)];
	sodium_bin2base64(password_hash_hash_base64, sodium_base64_ENCODED_LEN(crypto_generichash_BYTES, sodium_base64_VARIANT_URLSAFE), password_hash_hash, crypto_generichash_BYTES, sodium_base64_VARIANT_URLSAFE);

	auto it = state->intermediate_account_registrations.find(username.c_str());
	if (it == state->intermediate_account_registrations.end())
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = std::string("You must call registration/request_new_account_parameters before creating a new account")};
	char intermediate_salt_base64[sodium_base64_ENCODED_LEN(crypto_pwhash_SALTBYTES, sodium_base64_VARIANT_URLSAFE)];
	sodium_bin2base64(intermediate_salt_base64, sodium_base64_ENCODED_LEN(crypto_pwhash_SALTBYTES, sodium_base64_VARIANT_URLSAFE), it->second.value, crypto_pwhash_SALTBYTES, sodium_base64_VARIANT_URLSAFE);
	state->intermediate_account_registrations.erase(it);
	int user_id;
	try {
		user_id = state->db->createAccount(username.c_str(), std::move(password_hash_hash_base64), std::move(intermediate_salt_base64));
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
		.headers = std::unordered_map<std::string, std::string>{{"Set-Cookie", std::format("{}; HttpOnly", session_id_base64)}}
	};
}

FuzeHttp::Response requestLoginParameters(shared_state* state, FuzeHttp::Request req) {
	boost::json::value req_json;
	boost::json::string username;
	try {
		req_json = boost::json::parse(req.body());
		username = req_json.at("username").as_string();
	}
	catch(const std::exception& e) {
		return FuzeHttp::Response{.status = http::status::internal_server_error, .error_message = std::format("[registerAccount] {}", e.what())};
	}
	if (username.size() > ACCOUNT_MAX_USERNAME)
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = std::format("Username length {} is over the limit of {}", username.size(), ACCOUNT_MAX_USERNAME)};
	else if (username.empty())
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = std::string("Username cannot be empty")};

	// unsigned char intermediate_salt[crypto_pwhash_SALTBYTES];
	unsigned char salt[crypto_pwhash_SALTBYTES];
	int user_id = state->db->getAccountByUsername(username.c_str());
	if (user_id != BUILTIN_USERS::PUBLIC) {
		std::string intermediate_salt_base64 = state->db->getIntermediateSaltFromAccount(user_id);
		if (intermediate_salt_base64.length() == 0)
			return FuzeHttp::Response{.status = http::status::internal_server_error, .error_message = std::string("The intermediate salt is empty.")};
		crypto_generichash(salt, sizeof salt,
						   reinterpret_cast<const unsigned char*>(username.c_str()), username.size(),
						   reinterpret_cast<const unsigned char*>(intermediate_salt_base64.c_str()), intermediate_salt_base64.length());
	}
	else {
		crypto_generichash(salt, sizeof salt,
						   reinterpret_cast<const unsigned char*>(username.c_str()), username.size(),
						   reinterpret_cast<const unsigned char*>(state->getSecret()), sodium_base64_ENCODED_LEN(crypto_pwhash_SALTBYTES, sodium_base64_VARIANT_URLSAFE));
	}
	char salt_base64[sodium_base64_ENCODED_LEN(crypto_pwhash_SALTBYTES, sodium_base64_VARIANT_URLSAFE)];
	sodium_bin2base64(salt_base64, sodium_base64_ENCODED_LEN(crypto_pwhash_SALTBYTES, sodium_base64_VARIANT_URLSAFE), salt, crypto_pwhash_SALTBYTES, sodium_base64_VARIANT_URLSAFE);
	boost::json::object json = {
		{ "salt_base64", salt_base64 }
		// ,{ "pwhash_opslimit", state->client_pwhash_opslimit }
		// ,{ "pwhash_memlimit", state->client_pwhash_memlimit }
	};
	return FuzeHttp::Response{.status = http::status::ok, .json = std::move(json)};
}

FuzeHttp::Response login(shared_state* state, FuzeHttp::Request req) {
	boost::json::value req_json;
	boost::json::string username, password_hash_base64;
	std::cout << "createNewAccount called" << std::endl;
	try {
		req_json = boost::json::parse(req.body());
		username = req_json.at("username").as_string();
		password_hash_base64 = req_json.at("password_hash_base64").as_string();
	}
	catch(const std::exception& e) {
		std::cout << "JSON error" << std::endl;
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = std::format("[registerAccount] {}", e.what())};
	}
	if (username.size() > ACCOUNT_MAX_USERNAME)
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = std::format("Username length {} is over the limit of {}", username.size(), ACCOUNT_MAX_USERNAME)};
	else if (username.empty())
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = std::string("Username cannot be empty")};
	else if (state->db->getAccountByUsername(username.data()))
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = std::string("There already exists an account with this username.")};

	// hash of password hash in base64 is stored in DB
	unsigned char password_hash_hash[crypto_generichash_BYTES];
	crypto_generichash(password_hash_hash, crypto_generichash_BYTES, reinterpret_cast<const unsigned char*>(password_hash_base64.c_str()), password_hash_base64.size(), NULL, 0);
	char password_hash_hash_base64[sodium_base64_ENCODED_LEN(crypto_generichash_BYTES, sodium_base64_VARIANT_URLSAFE)];
	sodium_bin2base64(password_hash_hash_base64, sodium_base64_ENCODED_LEN(crypto_generichash_BYTES, sodium_base64_VARIANT_URLSAFE), password_hash_hash, crypto_generichash_BYTES, sodium_base64_VARIANT_URLSAFE);
	int user_id = state->db->getAccountByUsername(username.c_str());
	if (user_id != BUILTIN_USERS::PUBLIC) {
		if (state->db->userMatchesPassword(user_id, password_hash_hash_base64)) {
			unsigned char session_id_bytes[128/8];
			randombytes_buf(session_id_bytes, 128/8);
			char session_id_base64[sodium_base64_ENCODED_LEN(128/8, sodium_base64_VARIANT_URLSAFE)];
			sodium_bin2base64(session_id_base64, sodium_base64_ENCODED_LEN(128/8, sodium_base64_VARIANT_URLSAFE), session_id_bytes, 128/8, sodium_base64_VARIANT_URLSAFE);
			state->addSession(session_id_base64, {.user_id = user_id});
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
