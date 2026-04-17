#include "views.hpp"
#include "DatabaseConnection.hpp"
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
	// TODO check if username exists
	// Note: closed registration is required for resistance to account enumeration attacks.
	unsigned char intermediate_salt[128];
	unsigned char salt[crypto_generichash_BYTES];
	auto it = state->intermediate_account_registrations.find(username.c_str());
	if (it != state->intermediate_account_registrations.end())
		memcpy(intermediate_salt, it->second.value, 128);
	// else if username found in database, get the contini value
	else {
		randombytes_buf(intermediate_salt, 128);
		IntermediateSalt is;
		memcpy(is.value, intermediate_salt, 128);
		state->intermediate_account_registrations.emplace(std::string(username), std::move(is));
	}
	crypto_generichash(salt, sizeof salt,
					reinterpret_cast<const unsigned char*>(username.c_str()), username.size(),
					   intermediate_salt, sizeof intermediate_salt);
	char salt_base64[sodium_base64_ENCODED_LEN(crypto_generichash_BYTES, sodium_base64_VARIANT_URLSAFE_NO_PADDING)];
	sodium_bin2base64(salt_base64, sodium_base64_ENCODED_LEN(crypto_generichash_BYTES, sodium_base64_VARIANT_URLSAFE_NO_PADDING), salt, crypto_generichash_BYTES, sodium_base64_VARIANT_URLSAFE_NO_PADDING);
	boost::json::object json = {
		{ "salt_base64", salt_base64 },
		{ "pwhash_opslimit", state->client_pwhash_opslimit },
		{ "pwhash_memlimit", state->client_pwhash_memlimit }
	};
	return FuzeHttp::Response{.status = http::status::ok, .json = std::move(json)};
}

