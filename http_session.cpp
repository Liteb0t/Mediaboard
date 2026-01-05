//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/vinniefalco/CppCon2018
//

#include "http_session.hpp"
#include "permission_managed_object.hpp"
#include "websocket_session.hpp"
#include "field_lengths.h"
#include <boost/config.hpp>
#include <boost/filesystem.hpp>
#include <boost/locale.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <charconv>
#include <string>
#include <iostream>
#include <sstream>
#include <fstream>
#include <Magick++.h>
#include <nlohmann/json.hpp>
using json = nlohmann::json;

//------------------------------------------------------------------------------

// Return a reasonable mime type based on the extension of a file.
beast::string_view
mime_type(beast::string_view path)
{
    using beast::iequals;
    auto const ext = [&path]
    {
        auto const pos = path.rfind(".");
        if(pos == beast::string_view::npos)
            return beast::string_view{};
        return path.substr(pos);
    }();
    if(iequals(ext, ".htm"))  return "text/html";
    if(iequals(ext, ".html")) return "text/html";
    if(iequals(ext, ".php"))  return "text/html";
    if(iequals(ext, ".css"))  return "text/css";
    if(iequals(ext, ".txt"))  return "text/plain";
    if(iequals(ext, ".js"))   return "application/javascript";
    if(iequals(ext, ".json")) return "application/json";
    if(iequals(ext, ".xml"))  return "application/xml";
    if(iequals(ext, ".swf"))  return "application/x-shockwave-flash";
    if(iequals(ext, ".flv"))  return "video/x-flv";
    if(iequals(ext, ".png"))  return "image/png";
    if(iequals(ext, ".jpe"))  return "image/jpeg";
    if(iequals(ext, ".jpeg")) return "image/jpeg";
    if(iequals(ext, ".jpg"))  return "image/jpeg";
    if(iequals(ext, ".jxl"))  return "image/jxl";
    if(iequals(ext, ".gif"))  return "image/gif";
    if(iequals(ext, ".bmp"))  return "image/bmp";
    if(iequals(ext, ".ico"))  return "image/vnd.microsoft.icon";
    if(iequals(ext, ".tiff")) return "image/tiff";
    if(iequals(ext, ".tif"))  return "image/tiff";
    if(iequals(ext, ".svg"))  return "image/svg+xml";
    if(iequals(ext, ".svgz")) return "image/svg+xml";
    return "application/text";
}

const std::string forbidden_file_name_chars = "#?";

const std::set<std::string, std::less<>> image_formats = {"gif", "jpg", "jpeg", "jxl", "png", "webp", "bmp", "ico"};
const bool fileIsImage(std::string* file_name) {
	int dot_index = file_name->rfind('.');
	if (dot_index != std::string::npos) {
		std::string file_extension = file_name->substr(dot_index+1);
		if (image_formats.find(file_extension) != image_formats.end())
			return true;
		else
			return false;
	}
	else
		return false;
}

void sanitiseFileName(std::string* file_name) {
	for (int i = 0; i < file_name->length(); i++) {
		if (forbidden_file_name_chars.find((*file_name)[i]) != -1) {
			(*file_name)[i] = '_';
		}
	}
}
	
// URL decoding in C http://www.geekhideout.com/urlcode.shtml
char from_hex(char ch) {
	return std::isdigit(ch) ? ch - '0' : std::tolower(ch) - 'a' + 10;
}

// Append an HTTP rel-path to a local filesystem path.
// The returned path is normalized for the platform.
std::string
path_cat(
    beast::string_view base,
    beast::string_view path)
{
    if(base.empty())
        return std::string(path);
    std::string result(base);
#ifdef BOOST_MSVC
    char constexpr path_separator = '\\';
    if(result.back() == path_separator)
        result.resize(result.size() - 1);
    result.append(path.data(), path.size());
    for(auto& c : result)
        if(c == '/')
            c = path_separator;
#else
    char constexpr path_separator = '/';
    if(result.back() == path_separator)
        result.resize(result.size() - 1);
    result.append(path.data(), path.size());
#endif
    return result;
}

/*
template <typename T> auto api_response_T(T status, beast::string_view message) {
    http::response<http::empty_body> res;
	res.result(status);
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set("message", std::string(message));
    // res.keep_alive(req.keep_alive());
    res.prepare_payload();
    return res;
};
*/

// Return a response for the given request.
//
// The concrete type of the response message (which depends on the
// request), is type-erased in message_generator.
template <class Body, class Allocator>
http::message_generator
handle_request(
    // beast::string_view doc_root,
    boost::shared_ptr<shared_state> const& state,
    http::request<Body, http::basic_fields<Allocator>>&& req) {
    // Returns a bad request response
    auto const bad_request = [&req](beast::string_view why) {
        http::response<http::string_body> res{http::status::bad_request, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        res.body() = "Bad request; " + std::string(why);
        res.prepare_payload();
        return res;
    };

	auto const api_response = [&req](http::status status, beast::string_view message) {
        http::response<http::empty_body> res{status, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set("message", std::string(message));
        res.keep_alive(req.keep_alive());
        res.prepare_payload();
        return res;
	};

	auto const api_response_json = [&req](http::status status, nlohmann::json json) {
        http::response<http::string_body> res{status, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "application/json");
        res.keep_alive(req.keep_alive());
		res.body() = json.dump();
        res.prepare_payload();
        return res;
	};

    // Returns a not found response
    auto const not_found = [&req](beast::string_view target) {
        http::response<http::string_body> res{http::status::not_found, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        res.body() = "The resource '" + std::string(target) + "' was not found.";
        res.prepare_payload();
        return res;
    };

    // Returns a server error response
    auto const server_error = [&req](beast::string_view what) {
        http::response<http::string_body> res{http::status::internal_server_error, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        res.body() = "An error occurred: '" + std::string(what) + "'";
        res.prepare_payload();
        return res;
    };

	std::cout << "req target: " << req.target() << "\n";
	std::cout << "req version: " << req.version() << "\n";
	
	// URL decoding in C http://www.geekhideout.com/urlcode.shtml
	std::string decoded_url;
	decoded_url.reserve(req.target().length()+1);
	for (boost::string_view::const_iterator i = req.target().begin(), n = req.target().end(); i != n; i++) {
		std::string::value_type c = (*i);
		if (c == '%') {
			if (i+1 != n && i+2 != n) {
				decoded_url += from_hex(*(i+1)) << 4 | from_hex(*(i+2));
				i += 2;
			}
		}
		else if (c == '+')
			decoded_url += ' ';
		else
			decoded_url +=  c;
	}

	std::cout << "Decoded URL: " << decoded_url << std::endl;

    // Request path must be absolute and not contain "..".
    if( decoded_url.empty() ||
        decoded_url[0] != '/' ||
        decoded_url.find("..") != std::string::npos)
        return bad_request("Illegal request-target");

	// req_location excludes URL parameters (stuff after '?')
    std::string path, req_location;
	// int decoded_url_last_slash_index = decoded_url.rfind('/');
	int decoded_url_last_questionmark_index = decoded_url.rfind('?');
	if (decoded_url_last_questionmark_index != std::string::npos)
		req_location = decoded_url.substr(0, decoded_url_last_questionmark_index);
	else
		req_location = req.target();
	std::cout << "req_location: " << req_location << std::endl;

	auto const getNumberFromPath = [&req_location](int start_index) {
		std::size_t found = req_location.find_first_not_of("0123456789", start_index+1);

		if (found == start_index) {
			throw (std::string("Invalid group ID; cannot be empty."));
		}
		else if (req_location[start_index] == '/') {
			throw (std::string("First character cannot be a /. Try adding +1."));
		}
		else if (req_location[found] != '/') {
			throw (std::string("Invalid group ID; trailing '/' not found."));
		}
		else {
			int number_in_url;
			// Get number ID from URL substring
			std::cout << "Substring: " << req_location.substr(start_index, found - start_index) << std::endl;
			std::from_chars(req_location.substr(start_index, found - start_index).data(), req_location.substr(start_index, found - start_index).data() + req_location.substr(start_index, found - start_index).size(), number_in_url);
			return std::make_pair(number_in_url, found);
		}
		throw ("Program should not reach here.");
		return std::make_pair(-1, found);
	};

	auto const getUserFromToken	= [&req, &state]() {
		std::string token;
		// boost::intrusive::list_iterator<boost::intrusive::bhtraits<boost::beast::http::basic_fields<std::allocator<char>>::element, boost::intrusive::list_node_traits<void*>, boost::intrusive::normal_link, boost::intrusive::dft_tag, 1>, true> it = req.begin();
		// Iterates value_type. See: https://www.boost.org/doc/libs/boost_1_82_0/libs/beast/doc/html/beast/ref/boost__beast__http__basic_fields__value_type.html
		for (auto it = req.begin(); it != req.end(); it++) {
			if (it->name_string() == "Token") {
				token = it->value();
				std::cout << "Found Token in header. It is " << token << std::endl;
				break;
			}
		}
		if (token.length() < KEY_LENGTH+2) {
			// api_response(http::status::bad_request, std::string("Token too short"));
			throw(std::string("Token too short"));
		}
		std::string key = token.substr(0, KEY_LENGTH);
		std::string username = token.substr(KEY_LENGTH+1);
		if (key.length() == KEY_LENGTH) {
			int client_id;
			if (username == "Public") {
				client_id = static_cast<int>(BUILTIN_USERS::PUBLIC);
				return std::make_pair(client_id, key);
			}
			else {
				if (state->userExists(username)) {
					client_id = state->getIdFromUsername(username);
					if (state->checkUserKey(client_id, key))
						return std::make_pair(client_id, key);
					else
						throw (std::string("Key does not match user."));
				}
				else
					throw (std::string("User ") + username + " not found.");
			}
		}
		// client id -1 means there was an error
		return std::make_pair(-1, key);
	};

		// Make sure we can handle the method
	if 		(req.method() == http::verb::get) {
		bool is_media = false;
    	// Build the path to the requested file
		if (req_location.substr(0, 6) == "/media") {
			is_media = true;
			path = path_cat(state->doc_root(), decoded_url.substr(6));
		}
		else if (req_location.substr(0, 5) == "/api/") {
			std::pair<int, std::string> client;
			try {
				client = getUserFromToken();
			}
			catch(std::string error_text) {
				return api_response(http::status::bad_request, error_text);
			}
			http::response<http::string_body> res;
			res.set(http::field::content_type, "application/json");
			// std::cout << req_location.substr(5, 13) << std::endl;
			if (req_location.substr(5) == "threads/") {
				res.result(http::status::ok);
				// TODO check read permission
				res.body() = state->main_board()->dumpAllThreads(client.first);
			}
			else if (req_location.substr(5, 7) == "server/") {
				if (req_location.substr(12, 12) == "permissions/") {
					res.body() = state->dumpPermissions(client.first);
					res.result(http::status::ok);
				}
				else
					return api_response(http::status::bad_request, std::string("Bad URL. Do better next time."));
			}
			else if (req_location.substr(5, 7) == "thread/") {
				std::pair<int, int> thread_in_path = getNumberFromPath(12);
				if (state->main_board()->threadExists(thread_in_path.first)) {
					/*std::pair<int, std::string> client;
					try {
						client = getUserFromToken();
					}
					catch(std::string error_text) {
						return api_response(http::status::bad_request, error_text);
					}*/
					// TODO check for /permissions/ in URL
					if (req_location.substr(thread_in_path.second) == "/permissions/") {
						res.body() = state->main_board()->dumpPermissionsInThread(thread_in_path.first, client.first);
					}
					else {
						res.body() = state->main_board()->dumpPostsInThread(thread_in_path.first, client.second);
					}
					res.result(http::status::ok);
				}
				else {
					std::cout << "Error: thread '" << thread_in_path.first << "' does not exist" << std::endl;
					res.result(404);
				}
			}
			else if (req_location.substr(5, 6) == "group/") { // TODO add /members/ to end of URL check
				std::size_t found = req_location.find_first_not_of("0123456789", 11);
				if (req_location[found] != '/') {
					return api_response(http::status::bad_request, std::string("Invalid group ID; trailing '/' not found."));
				}
				else if (found == 11) {
					return api_response(http::status::bad_request, std::string("Invalid group ID; cannot be empty."));
				}
				else {
					int group_in_url;
					// Get group ID from URL substring
					std::from_chars(req_location.substr(11, found).data(), req_location.substr(11, found).data() + req_location.substr(11, found).size(), group_in_url);
					std::cout << "group_id_url: " << group_in_url << std::endl;
					if (state->groupExists(group_in_url)) {
						// res.body() = state->dumpMembersInGroup(group_in_url);
						res.body() = state->dumpMembersInGroupAsArray(group_in_url);
						res.result(http::status::ok);
					}
					else
						return api_response(http::status::bad_request, std::string("Group '") + std::to_string(group_in_url) + "' not found.");
				}
			}
			else if (req_location.substr(5) == "groups/") {
				std::string token;
				for (auto it = req.begin(); it != req.end(); it++) {
					if (it->name_string() == "Token") {
						token = it->value();
						std::cout << "Found Token in header. It is " << token << std::endl;
						break;
					}
				}
				if (token.length() < KEY_LENGTH+2) {
					return api_response(http::status::bad_request, "Token too short");
				}
				std::string key = token.substr(0, KEY_LENGTH);
				std::string username = token.substr(KEY_LENGTH+1);
				std::cout << "[http_session] Key : Username from token: " << key << " : " << username << std::endl;
				// TODO check if username and key exist + are correct
				res.body() = state->dumpAllGroups(username, key);
				res.result(http::status::ok);
			}
			else if (req_location.substr(5) == "users/") {
				/*std::pair<int, std::string> client;
				try {
					client = getUserFromToken();
				}
				catch(std::string error_text) {
					return api_response(http::status::bad_request, error_text);
				}*/
				// TODO check if username and key exist + are correct
				res.body() = state->dumpAllUsers(client.first);
				int user_rank = state->getUserRank(client.first);
				res.set("Client-Rank", std::to_string(user_rank));
				res.result(http::status::ok);
			}
			else {
				return api_response(http::status::bad_request, "That API endpoint does not exist");
			}
			res.prepare_payload();
			return res;
		}
		else if (req_location.back() == '/') {
			path = "index.html";
			std::cout << "/path: " << path << std::endl;
		}
		else {
			// This is used to access files in the server's directory
			path = req_location.substr(1);
		}

    	// Attempt to open the file
    	beast::error_code ec;
    	http::file_body::value_type body;
		std::cout << "Opening path: " << path << std::endl;
    	body.open(path.c_str(), beast::file_mode::scan, ec);

    	// Handle the case where the file doesn't exist
    	if (ec == boost::system::errc::no_such_file_or_directory)
    	    return not_found(req.target());
		else if (ec) // Handle an unknown error
			return server_error(ec.message());

		// Check if path leads to a directory
		boost::filesystem::path filesystem_path(path);
		if (!boost::filesystem::is_regular_file(filesystem_path))
			return bad_request("Is a directory.");

		std::string filename;
		if (is_media) {
			int filename_start_index = req.target().rfind("/") + 1;
			filename = req.target().substr(filename_start_index, req.target().length() - filename_start_index);
			int filename_extension_index;
			if ((filename_extension_index = filename.rfind(".")) == -1) {
				filename_extension_index = filename.size();
			}
			if (filename_extension_index >= 36) {
				filename.erase(filename_extension_index - 36, 36);
			}
			// We know it's not a user-submitted file when the filename is too short to include a UUID.
			else
				is_media = false;
			std::cout << "Is media. Filename: " << filename << std::endl;
		}

    	// Cache the size since we need it after the move
    	auto const size = body.size();

    	// Respond to HEAD request
    	if(req.method() == http::verb::head)
    	{
    	    http::response<http::empty_body> res{http::status::ok, req.version()};
    	    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    	    res.set(http::field::content_type, mime_type(path));
    	    res.content_length(size);
    	    res.keep_alive(req.keep_alive());
    	    return res;
    	}

    	// Respond to GET request
    	http::response<http::file_body> res{
    	    std::piecewise_construct,
    	    std::make_tuple(std::move(body)),
    	    std::make_tuple(http::status::ok, req.version())
		};
		// if (is_media) {
			// Only set when the filename is long enough to include the UUID.
			// In other words, we know it's a user-uploaded file.
		// 	res.set("Content-Disposition", "attachment; filename=\"" + filename + "\"");
		// }
    	res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    	res.set(http::field::content_type, mime_type(path));
    	res.content_length(size);
    	res.keep_alive(req.keep_alive());
    	return res;
	}
	else if (req.method() == http::verb::post) {
		if (req_location == "/api/message/" || req_location == "/api/thread/") {
			std::pair<int, std::string> client;
			try {
				client = getUserFromToken();
			}
			catch(std::string error_text) {
				return api_response(http::status::bad_request, error_text);
			}
			http::response<http::empty_body> res;
			json request_json = json::parse(req.body());
			bool is_thread;
			json post_json;
			if (req_location == "/api/thread/") {
				is_thread = true;
				if (request_json.contains("thread") && 
					request_json["thread"].contains("post_zero")) {
					post_json = request_json["thread"]["post_zero"];
				}
				else {
					res.result(400);
					res.set("message", "\"thread\" or \"post_zero\" JSON field(s) missing.");
					res.prepare_payload();
					return res;
				}
			}
			else {
				is_thread = false;
				if (request_json.contains("post")) {
					post_json = request_json["post"];
				}
				else {
					res.result(400);
					res.set("message", "\"post\" JSON field missing.");
					res.prepare_payload();
					return res;
				}
			}
			if (	post_json.contains("files") &&
					post_json.contains("name") &&
					post_json.contains("content")) {
				if (post_json["files"].size() > 4) {
					std::cerr << "Denied: More than 4 files in message\n";
					res.set("message", "More than 4 files attatched.");
					res.result(400);
				}
				else {
					std::string message_content = post_json["content"].template get<std::string>();
					if ((message_content.length() > 0 || post_json["files"].size() > 0) && message_content.length() < POST_MAX_CONTENT) {
						if (is_thread) {
							// if (state->userHasPermission())
							request_json["thread"]["post_zero"]["key"] = client.second; // key is to identify the author of a post
							int new_thread_id = state->main_board()->createThread(request_json["thread"]);
							res.set("New-Thread-Id", std::to_string(new_thread_id));
							res.result(201);
						}
						else {
							int new_message_thread_id = post_json["thread_id"].template get<int>();
							// TODO authorize user
							if (state->main_board()->threadExists(new_message_thread_id)) {
								post_json["key"] = client.second;  // key is to identify the author of a post
								int new_message_id = state->main_board()->createPost(post_json);
								std::string new_message_dump = state->main_board()->dumpPost(new_message_thread_id, new_message_id, client.second);
								state->sendToThread(new_message_dump, new_message_thread_id);
								res.result(201);
							}
							else {
								std::cerr << "Couldn't create message because the thread with ID " << new_message_thread_id << " does not exist" << std::endl;
								res.set("message", "thread with ID " + std::to_string(new_message_thread_id) + " does not exist");
								res.result(400);
							}
						}
					}
					else {
						res.set("message", "The post does not meet the constraints set by the server.\nThis could mean that the message content was empty and no files were uploaded, or the message content is too long.");
						res.result(400);
					}
				}
			}
			else {
				res.set("message", "One or more JSON fields missing in post.");
				res.result(400);
			}
			res.prepare_payload();
			return res;
		}
		// The URL extends past /thread/, used for permission management
		else if (req_location.substr(0, 12) == "/api/thread/") {
			std::pair<int, std::string> client;
			try {
				client = getUserFromToken();
			}
			catch(std::string error_text) {
				return api_response(http::status::bad_request, error_text);
			}
			std::pair<int, int> thread_in_url;
			try {
				thread_in_url = getNumberFromPath(12);
			}
			catch(std::string error_text) {
				return api_response(http::status::bad_request, std::string("Bad URL."));
			}

			boost::shared_ptr<Thread> thread = state->getThread(0, thread_in_url.first);

			if (req_location.substr(thread_in_url.second, 19) == "/permissions/group/") {
				int group_in_url;
				try {
					group_in_url = getNumberFromPath(thread_in_url.second+19).first;
				}
				catch(std::string error_text) {
					return api_response(http::status::bad_request, std::string("Bad URL."));
				}
				if (thread->userHasPermission(client.first, PERMISSION::MANAGE_PERMISSIONS)) {
					state->main_board()->addGroupPermissionCollectionToThread(group_in_url, thread_in_url.first);
					// thread->addGroupPermissionCollection(group_in_url);
					return api_response(http::status::ok, std::string("Group permission collection created"));
				}
				else
					return api_response(http::status::forbidden, std::string("User lacks permission MANAGE_PERMISSIONS"));
			}
			else if (req_location.substr(thread_in_url.second, 18) == "/permissions/user/") {
				int user_in_url = getNumberFromPath(thread_in_url.second+18+1).first;
				// TODO check permission
				// if (shared_state->userHasPermission()
				thread->addUserPermissionCollection(user_in_url);
				return api_response(http::status::ok, std::string("User permission collection created"));
			}
			else
				return api_response(http::status::not_found, std::string("/api/server sub-URL not found"));
		}
		else if (req.target() == "/api/create_group/") {
			http::response<http::empty_body> res;
			json request_json;
			try {
				request_json = json::parse(req.body());
				if (	request_json.contains("group")
					 && request_json["group"].contains("name")
				   ) {
					std::string token = "";
					for (auto it = req.begin(); it != req.end(); it++) {
						if (it->name_string() == "Token") {
							token = it->value();
							std::cout << "Found Token in header. It is " << token << std::endl;
							break;
						}
					}
					if (token.length() < KEY_LENGTH+2) {
						return api_response(http::status::bad_request, "Token badly formed or missing from request header.");
					}
					std::string key = token.substr(0, KEY_LENGTH);
					std::string username = token.substr(KEY_LENGTH+1);
					std::string new_group_name = request_json["group"]["name"].template get<std::string>();
					BasicResponse function_response = state->createGroup(username, key, new_group_name);
					return api_response(function_response.status, function_response.message);
					// TODO send new group ID to frontend
					// int new_group_id = state->createGroup(request_json);
					// res.set("New-Group-ID", std::to_string(new_group_id));
					// res.result(201);
					// res.prepare_payload();
					// return res;
				}
				else {
					return api_response(http::status::bad_request, "One or more JSON fields missing in group.");
				}
			}
			catch (const json::exception& exception) {
				return api_response(http::status::bad_request, exception.what());
			}
		}
		// For now assume the URL ends with add_groups/
		else if (req_location.substr(0, 10) == "/api/user/") {
			std::size_t found = req_location.find_first_not_of("0123456789", 11);
			if (req_location[found] != '/') {
				return api_response(http::status::bad_request, std::string("Invalid user ID; trailing '/' not found."));
			}
			else if (found == 10) {
				return api_response(http::status::bad_request, std::string("Invalid user ID; cannot be empty."));
			}
			else {
				std::string token = "";
				for (auto it = req.begin(); it != req.end(); it++) {
					if (it->name_string() == "Token") {
						token = it->value();
						std::cout << "Found Token in header. It is " << token << std::endl;
						break;
					}
				}
				if (token.length() < KEY_LENGTH+2) {
					return api_response(http::status::bad_request, "Token badly formed or missing from request header.");
				}
				std::string key = token.substr(0, KEY_LENGTH);
				std::string username = token.substr(KEY_LENGTH+1);

				int user_in_url;
				// Get group ID from URL substring
				std::from_chars(req_location.substr(10, found).data(), req_location.substr(10, found).data() + req_location.substr(10, found).size(), user_in_url);
				std::cout << "user_id_url: " << user_in_url << std::endl;

				json request_json;
				std::vector<int> groups_to_add;
				try {
					request_json = json::parse(req.body());
					groups_to_add = request_json["groups_by_id"].template get<std::vector<int>>();
				}
				catch (const json::exception& exception) {
					return api_response(http::status::bad_request, exception.what());
				}

				BasicResponse function_response = state->addUserToGroups(username, key, user_in_url, groups_to_add);
				return api_response(function_response.status, function_response.message);
			}
		}
		else if (req.target().substr(5,9) == "register/") {
			json request_json;
			try {
				request_json = json::parse(req.body());
			}
			catch (const json::exception& exception) {
				return api_response(http::status::bad_request, exception.what());
			}
			BasicResponse function_response = state->createAccount(request_json);
			if (function_response.json) {
				return api_response_json(function_response.status, function_response.json.get());

			}
			else // The request was invalid
				return api_response(function_response.status, function_response.message);
		}
		else if (req.target().substr(5,6) == "login/") {
			json request_json;
			try {
				request_json = json::parse(req.body());
			}
			catch (const json::exception& exception) {
				return api_response(http::status::bad_request, exception.what());
			}
			BasicResponse function_response = state->getKeyFromPassword(request_json);
			if (function_response.json) {
				std::cout << "There is function response JSON" << std::endl;
				return api_response_json(function_response.status, function_response.json.get());

			}
			else { // The request was invalid
				std::cout << "There is NO function response JSON" << std::endl;
				return api_response(function_response.status, function_response.message);
			}
		}
		else if (req_location.substr(0, 12) == "/api/server/") {
			std::cout << req_location.substr(12, 18) << std::endl;
			if (req_location.substr(12, 18) == "permissions/group/") {
				int group_in_url = getNumberFromPath(30).first;
				// TODO get user from token and check his/her permission
				// if (shared_state->userHasPermission()
				state->addGroupPermissionCollection(group_in_url);
				return api_response(http::status::ok, std::string("Group permission collection created"));
				// return api_response(http::status::not_implemented, std::string("Group ID: " + std::to_string(group_in_url) + " but action not implemented"));
			}
			else if (req_location.substr(12, 17) == "permissions/user/") {
				int user_in_url = getNumberFromPath(29).first;
				// TODO get user from token and check his/her permission
				// if (shared_state->userHasPermission()
				state->addUserPermissionCollection(user_in_url);
				return api_response(http::status::ok, std::string("User permission collection created"));
			}
			else
				return api_response(http::status::not_found, std::string("/api/server sub-URL not found"));
		}
		/*
		else if (decoded_url == "/api/change_password/") {
			http::response<http::empty_body> res;
			json request_json = json::parse(req.body());
			if (	   request_json.contains("username")
			 		&& request_json.contains("old_password")
					&& request_json.contains("new_password")
					) {
				std::string username = request_json["username"].template get<std::string>();
				std::string old_password = request_json["old_password"].template get<std::string>();
				std::string new_password = request_json["new_password"].template get<std::string>();
				char change_password_result = db_change_password(username.c_str(), old_password.c_str(), new_password.c_str());
				if (change_password_result == 't') {
					res.result(http::status::ok);
				}
				else if (change_password_result == '\0') {
					res.result(400);
					res.set("message", "No account with this username exists.");
				}
				else {
					res.result(400);
					res.set("message", "Password change operation failed.");
				}
			}
			else {
				res.result(400);
				res.set("message", "One or more JSON fields missing.");
			}
			res.prepare_payload();
			return res;
		}
		*/
		else if (req.target() == "/api/upload/") {
			// request_parser<empty_body> req_parser;
			// std::string content_dispo =  req.get()[http::field::content_disposition] << std::endl;
			// std::cout << req.body() << std::endl;
			// std::string req_string = req.body();
			// std::cout << req_string << std::endl;
			std::istringstream req_stream(req.body());
			std::string req_line;
			std::getline(req_stream, req_line);
			std::string req_terminator = req_line.substr(0, req_line.length()-1) + "--\r";
			// std::cout << "Request ID: " << req_terminator << std::endl;
			std::string out_filename;
			bool empty_line = false;
			while (!empty_line) {
				std::getline(req_stream, req_line, '\n');
				if (req_line == "\r") {
					empty_line = true;
				}
				else if (req_line.substr(0, 19) == "Content-Disposition") {
					int filename_i;
					filename_i = req_line.find("filename", 20) + 10;
					if (filename_i != std::string::npos) {
						int filename_end_i;
						if ((filename_end_i = req_line.find(";", filename_i)) == std::string::npos) {
							filename_end_i = req_line.length() - filename_i - 2;
						}
						std::cout << filename_end_i << std::endl;
						out_filename = req_line.substr(filename_i, filename_end_i);
						std::cout << "out_filename: " << out_filename << std::endl;
					}
				}
				// else if (req_line.substr(0, 13) == "Content-Type") {
				// 	int boundary_i;
				// 	if ((boundary_i = req_line.find("boundary", 13)) != std::string::npos) {
				// 		std::cout << req_line.substr(boundary_i+1, req_line.length()) << std::endl;
				// 	}
				// }
				// std::cout << "line: " << i++ << std::endl << req_line << std::endl;
			}
			if (out_filename.empty()) {
				// out_filename = "UNKNOWN_NAME";
				std::cerr << "Error! file name not found in POST header" << std::endl;
				return server_error("Could not determine filename");
			}
			else if (out_filename.length() > POST_MAX_FILE_NAME) {
				http::response<http::empty_body> res;
				res.result(400);
				res.set("message", "File name length exceeds the server-defined limit of " + std::to_string(POST_MAX_FILE_NAME) + ".");
				res.prepare_payload();
				return res;
			}
			sanitiseFileName(&out_filename);
			std::cout << "Sanitised out_filename: " << out_filename << std::endl;

			// Add UUID to filename
			boost::uuids::uuid u = boost::uuids::random_generator()();
			std::string uuid_str = boost::uuids::to_string(u);
			int filename_uuid_index;
			if ((filename_uuid_index = out_filename.rfind(".")) == -1) {
				filename_uuid_index = out_filename.size();
			}
			out_filename.insert(filename_uuid_index, uuid_str);

			// Write to the file
			std::ofstream outfile(state->doc_root() + out_filename, std::ios::binary);
			bool is_initial_line = true;
			bool previous_line_ends_with_carriage_return = false;
			while (std::getline(req_stream, req_line)) {
				std::cout << req_line << std::endl;
				std::cout << req_line.length() << ", " << req_terminator.length() << std::endl;
				if (req_line != req_terminator) {
					if (previous_line_ends_with_carriage_return) {
						previous_line_ends_with_carriage_return = false;
						outfile << "\r";
					}
					if (!is_initial_line)
						outfile << "\n";
					if (req_line.length() == 0)
						continue;
					else if (req_line.back() == '\r') {
						previous_line_ends_with_carriage_return = true;
						outfile << req_line.substr(0, req_line.length() - 1);
					}
					else
						outfile << req_line;
					is_initial_line = false;
				}
				else {
					break;
				}
			}
			std::cout << "Finished reading data" << std::endl;
			try {
				outfile.exceptions(outfile.failbit);
				outfile.close();
			}
			catch (const std::ios_base::failure& exception) {
				std::stringstream error_message;
				error_message
					<< "Reason: " << exception.what() << '\n'
					<< "Error code: " << exception.code() << "\n";
				std::cerr << "Exception thrown when attempting to save uploaded file.\n" << error_message.str();
				http::response<http::empty_body> res{http::status::internal_server_error, req.version()};
				res.set("message", error_message.str());
    			res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
				// res.set("File-Name-UTF-8", filename_utf_8);
				res.set("Access-Control-Allow-Origin", "*");
				res.set("Access-Control-Allow-Headers", "Origin, X-Requested-With, Content-Type, Accept, File-Name");
    			res.set(http::field::content_type, "text/plain; charset=utf-8");
    			res.content_length(0);
    			res.keep_alive(req.keep_alive());
    			return res;
			}
			std::cout << "END OF FILE" << std::endl;

			// Write thumbnail
			if (fileIsImage(&out_filename)) {
				Magick::Image thumbnail;
				try {
					thumbnail.read(state->doc_root() + out_filename);
					thumbnail.strip(); // Removes metadata
					thumbnail.resize("150x150");
					thumbnail.quality(50);
					thumbnail.write(state->doc_root() + "thumbnails/THUMBNAIL_" + out_filename + ".jxl");
				}
				catch (Magick::Error& magick_error) {
					std::cerr << "[Magick++] ERROR: " << magick_error.what() << std::endl << "Thumbnail will therefore not be made." << std::endl;
				}
			}

			// std::string filename_utf_8 = boost::locale::conv::to_utf(out_filename, "UTF-8");
    		http::response<http::empty_body> res{http::status::accepted, req.version()};
    		res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
			res.set("File-Name", out_filename);
			// res.set("File-Name-UTF-8", filename_utf_8);
			res.set("Access-Control-Allow-Origin", "*");
			res.set("Access-Control-Allow-Headers", "Origin, X-Requested-With, Content-Type, Accept, File-Name");
    		res.set(http::field::content_type, "text/plain; charset=utf-8");
    		res.content_length(0);
    		res.keep_alive(req.keep_alive());
    		return res;
		}
		else {
			std::cout << "Unknown target: " << req.target() << std::endl;
			std::cout << "Unknown target: " << decoded_url << std::endl;
			http::response<http::empty_body> res;
			res.result(404);
			res.prepare_payload();
			return res;
		}
	}
	else if (req.method() == http::verb::put) {
		std::pair<int, std::string> client;
		try {
			client = getUserFromToken();
		}
		catch(std::string error_text) {
			return api_response(http::status::bad_request, error_text);
		}
		if (req.target() == "/api/group_heirarchy/") {
			// TODO: authorize user with key
			json request_json;
			try {
				request_json = json::parse(req.body());
			}
			catch (const json::exception& exception) {
				return api_response(http::status::bad_request, exception.what());
			}

			if (request_json.contains("new_group_heirarchy")
					 && request_json.contains("username")
					 && request_json.contains("key")
					) {
				std::vector<int> new_group_heirarchy;
				std::string username, key;
				try {
					new_group_heirarchy = request_json["new_group_heirarchy"].template get<std::vector<int>>();
					username = request_json["username"].template get<std::string>();
					key = request_json["key"].template get<std::string>();
				}
				catch (const json::exception& exception) {
					return api_response(http::status::bad_request, exception.what());
				}
				BasicResponse function_response = state->setGroupHeirarchy(username, key, new_group_heirarchy);
				return api_response(function_response.status, function_response.message);
			}
			else
				return api_response(http::status::bad_request, "ordered_groups not found in JSON request");
		}
		else if (req_location.substr(0, 12) == "/api/server/") {
			std::cout << req_location.substr(12, 18) << std::endl;
			bool is_group;
			int group_or_user_in_url;
			int rank_of_user_or_group;
			if (req_location.substr(12, 18) == "permissions/group/") {
				group_or_user_in_url = getNumberFromPath(30).first;
				is_group = true;
				rank_of_user_or_group = state->getGroupRank(group_or_user_in_url);
			}
			else if (req_location.substr(12, 17) == "permissions/user/") {
				group_or_user_in_url = getNumberFromPath(29).first;
				is_group = false;
				rank_of_user_or_group = state->getUserRank(group_or_user_in_url);
			}
			else
				return api_response(http::status::not_found, std::string("/api/server sub-URL not found"));

			json request_json;
			int _permission_number, _permission_setting;
			try {
				request_json = json::parse(req.body());
				_permission_number = request_json["permission"].template get<int>();
				_permission_setting = request_json["setting"].template get<int>();
			}
			catch (const json::exception& exception) {
				return api_response(http::status::bad_request, exception.what());
			}
			// TODO check if ints are in range
			PERMISSION permission = static_cast<PERMISSION>(_permission_number);
			THREE_STATE_SETTING permission_setting = static_cast<THREE_STATE_SETTING>(_permission_setting);
			if (state->userHasPermission(client.first, PERMISSION::MANAGE_PERMISSIONS) && state->getUserRank(client.first) < rank_of_user_or_group) {
				std::cout << "permission_setting: " << static_cast<int>(permission_setting) << std::endl;
				if (is_group) {
					state->setGroupPermission(group_or_user_in_url, permission, permission_setting);
					return api_response(http::status::ok, std::string("Group permission updated"));
				}
				else {
					state->setUserPermission(group_or_user_in_url, permission, permission_setting);
					return api_response(http::status::ok, std::string("User permission updated"));
				}
			}
			else
				return api_response(http::status::forbidden, std::string("Permission denied for this client"));
		}
		else if (req_location.substr(0, 12) == "/api/thread/") {
			// Assume we edit permissions, because that is the only feature implemented for PUT /api/thread/
			bool is_group;
			int group_or_user_in_url;
			int rank_of_user_or_group;
			std::pair<int, int> thread_in_url;
			try {
				thread_in_url = getNumberFromPath(12);
			}
			catch(std::string error_text) {
				return api_response(http::status::bad_request, std::string("Bad URL."));
			}
			boost::shared_ptr<Thread> thread = state->getThread(0, thread_in_url.first);

			if (req_location.substr(thread_in_url.second, 19) == "/permissions/group/") {
				group_or_user_in_url = getNumberFromPath(30).first;
				is_group = true;
				rank_of_user_or_group = state->getGroupRank(group_or_user_in_url);
			}
			else if (req_location.substr(thread_in_url.second, 18) == "/permissions/user/") {
				group_or_user_in_url = getNumberFromPath(29).first;
				is_group = false;
				rank_of_user_or_group = state->getUserRank(group_or_user_in_url);
			}
			else
				return api_response(http::status::not_found, std::string("/api/thread/ sub-URL not found"));

			json request_json;
			int _permission_number, _permission_setting;
			try {
				request_json = json::parse(req.body());
				_permission_number = request_json["permission"].template get<int>();
				_permission_setting = request_json["setting"].template get<int>();
			}
			catch (const json::exception& exception) {
				return api_response(http::status::bad_request, exception.what());
			}
			// TODO check if ints are in range
			PERMISSION permission = static_cast<PERMISSION>(_permission_number);
			THREE_STATE_SETTING permission_setting = static_cast<THREE_STATE_SETTING>(_permission_setting);
			if (thread->userHasPermission(client.first, PERMISSION::MANAGE_PERMISSIONS) && state->getUserRank(client.first) < rank_of_user_or_group) {
				std::cout << "permission_setting: " << static_cast<int>(permission_setting) << std::endl;
				if (is_group) {
					thread->setGroupPermission(group_or_user_in_url, permission, permission_setting);
					return api_response(http::status::ok, std::string("Group permission updated"));
				}
				else {
					thread->setUserPermission(group_or_user_in_url, permission, permission_setting);
					return api_response(http::status::ok, std::string("User permission updated"));
				}
			}
			else
				return api_response(http::status::forbidden, std::string("Permission denied for this client"));
		}
		else
			return not_found(req.target());
	}
	else if (req.method() == http::verb::delete_) {
		if (req_location.substr(0, 10) == "/api/post/") {
			http::response<http::empty_body> res;
			int thread_id, message_id;
			bool is_thread;
			std::pair<int, int> thread_in_url/*, message_in_url*/;
			thread_in_url = getNumberFromPath(10);
			thread_id = thread_in_url.first;
			if (req_location.length() > thread_in_url.second) {
				is_thread = false;
				try {
					message_id = getNumberFromPath(thread_in_url.second+1).first;
				}
				catch(std::string error_text) {
					return api_response(http::status::bad_request, std::string("Bad URL."));
				}
				std::cout << "Post to delete is not a thread. post ID: #" << thread_id << '/' << message_id << std::endl;
			}
			else {
				is_thread = true;
			}
			if (state->main_board()->threadExists(thread_id)) {
				std::pair<int, std::string> client;
				try {
					client = getUserFromToken();
				}
				catch(std::string error_text) {
					return api_response(http::status::bad_request, error_text);
				}
				bool is_administrator = db_key_matches_account(client.second.c_str(), "Administrator");
				if (is_thread) {
					if (is_administrator) {
						state->main_board()->deleteThread(thread_id);
						res.result(http::status::ok);
					}
					else
						return bad_request("Denied: User must be administrator to delete a thread");
				}
				else if (is_administrator || state->main_board()->keyMatchesMessageInThread(client.second.c_str(), message_id, thread_id)) {
					if (state->main_board()->messageExistsInThread(message_id, thread_id)) {
						state->main_board()->deleteMessageFromThread(message_id, thread_id);
						res.result(200);
					}
					else {
						std::cout << "Message " << message_id << " does not exist in thread " << thread_id << std::endl;
						res.result(400);
					}
				}
				else {
					std::cout << "Permission denied for post deletion.\n";
					res.result(400);
				}
			}
			else {
				std::cout << "Thread " << thread_id << " does not exist\n";
				res.result(400);
			}
			res.prepare_payload();
			return res;
		}
		else if (req.target().substr(5, 6) == "group/") {
			std::pair<int, std::string> client;
			try {
				client = getUserFromToken();
			}
			catch(std::string error_text) {
				return api_response(http::status::bad_request, error_text);
			}

			std::pair<int, int> group_in_url = getNumberFromPath(11);
			std::cout << "Group in URL: " << group_in_url.first << ", req_location.length(): " << req_location.length() << ", group.second: " << group_in_url.second << std::endl;
			if (state->userHasPermission(client.first, PERMISSION::MANAGE_PERMISSIONS) && state->getUserRank(client.first) < state->getGroupRank(group_in_url.first)) {
				if (req_location.length() > group_in_url.second+1) {
					std::cout << req_location.substr(group_in_url.second+1, 7) << std::endl;
					if (req_location.substr(group_in_url.second+1, 7) == "member/") {
						std::pair<int, int> member_in_url = getNumberFromPath(group_in_url.second+1+7);
						state->removeUserFromGroup(member_in_url.first, group_in_url.first);
						return api_response(http::status::ok, std::string("Member dismissed from group"));
						// return api_response(http::status::not_implemented, std::string("Group ID: " + std::to_string(group_in_url.first) + " but DELETE MEMBER action not implemented"));
					}
					else
						return api_response(http::status::bad_request, std::string("Unknown permission group target: ") + std::string(req.target()));
				}
				else {
					state->eraseGroup(group_in_url.first);
					return api_response(http::status::ok, std::string("Group deleted"));
				}
			}
			else
				return api_response(http::status::unauthorized, std::string("Permission denied for this client"));
			// BasicResponse function_response = state->deleteGroup(client.first, group_in_url.first);
			// return api_response(function_response.status, function_response.message);
		}
		else if (req_location.substr(0, 12) == "/api/server/") {
			std::cout << req_location.substr(12, 18) << std::endl;
			if (req_location.substr(12, 18) == "permissions/group/") {
				// TODO check if group exists
				std::pair<int, std::string> client;
				try {
					client = getUserFromToken();
				}
				catch(std::string error_text) {
					return api_response(http::status::bad_request, error_text);
				}
				std::pair<int, int> group_in_url = getNumberFromPath(30);
				if (state->userHasPermission(client.first, PERMISSION::MANAGE_PERMISSIONS) && state->getUserRank(client.first) < state->getGroupRank(group_in_url.first)) {
					state->removeGroupPermissionCollection(group_in_url.first);
					return api_response(http::status::ok, std::string("Group permission collection removed"));
				}
				else
					return api_response(http::status::unauthorized, std::string("Couldn't remove group permission collection; Permission denied for this client"));
				// return api_response(http::status::not_implemented, std::string("Group ID: " + std::to_string(group_in_url.first) + " but action not implemented"));
			}
			else if (req_location.substr(12, 17) == "permissions/user/") {
				int user_in_url = getNumberFromPath(29).first;
				// TODO check if user exists
				// TODO get user from token and check his/her permission
				// if (shared_state->userHasPermission()
				state->removeUserPermissionCollection(user_in_url);
				return api_response(http::status::ok, std::string("User permission collection removed"));
			}
			else
				return api_response(http::status::not_found, std::string("/api/server sub-URL not found"));
		}
		else if (req_location.substr(0, 12) == "/api/thread/") {
			std::pair<int, std::string> client;
			try {
				client = getUserFromToken();
			}
			catch(std::string error_text) {
				return api_response(http::status::bad_request, error_text);
			}
			// Assume we edit permissions, because that is the only feature implemented for PUT /api/thread/
			bool is_group;
			int group_or_user_in_url;
			int rank_of_user_or_group;
			std::pair<int, int> thread_in_url;
			try {
				thread_in_url = getNumberFromPath(12);
			}
			catch(std::string error_text) {
				return api_response(http::status::bad_request, std::string("Bad URL."));
			}
			boost::shared_ptr<Thread> thread = state->getThread(0, thread_in_url.first);

			if (req_location.substr(thread_in_url.second, 19) == "/permissions/group/") {
				group_or_user_in_url = getNumberFromPath(30).first;
				is_group = true;
				rank_of_user_or_group = state->getGroupRank(group_or_user_in_url);
			}
			else if (req_location.substr(thread_in_url.second, 18) == "/permissions/user/") {
				group_or_user_in_url = getNumberFromPath(29).first;
				is_group = false;
				rank_of_user_or_group = state->getUserRank(group_or_user_in_url);
			}
			else
				return api_response(http::status::not_found, std::string("/api/thread/ sub-URL not found"));

			if (thread->userHasPermission(client.first, PERMISSION::MANAGE_PERMISSIONS) && state->getUserRank(client.first) < rank_of_user_or_group) {
				if (is_group) {
					state->main_board()->removeGroupPermissionCollectionFromThread(group_or_user_in_url, thread_in_url.first);
					return api_response(http::status::ok, std::string("Group permission deleted"));
				}
				else {
					state->main_board()->removeUserPermissionCollectionFromThread(group_or_user_in_url, thread_in_url.first);
					return api_response(http::status::ok, std::string("User permission deleted"));
				}
			}
			else
				return api_response(http::status::forbidden, std::string("Permission denied for this client"));
		}
		else {
			return api_response(http::status::bad_request, std::string("Unknown target: ") + std::string(req.target()));
		}
	}
	else {
        http::response<http::empty_body> res{http::status::not_implemented, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.keep_alive(req.keep_alive());
        res.prepare_payload();
        return res;
	}
}

//------------------------------------------------------------------------------

http_session::
http_session(
    tcp::socket&& socket,
    boost::shared_ptr<shared_state> const& state)
    : stream_(std::move(socket))
    , state_(state)
{
}

void
http_session::
run()
{
    do_read();
}

// Report a failure
void
http_session::
fail(beast::error_code ec, char const* what)
{
    // Don't report on canceled operations
    if(ec == net::error::operation_aborted)
        return;

    std::cerr << what << ": " << ec.message() << "\n";
}

void
http_session::
do_read()
{
    // Construct a new parser for each message
    parser_.emplace();

    // Apply a reasonable limit to the allowed size
    // of the body in bytes to prevent abuse.
	// 6MB would match 4chins
	// This is 100MB
    parser_->body_limit(100 << 20);

    // Set the timeout.
    stream_.expires_after(std::chrono::seconds(30));

    // Read a request
    http::async_read(
        stream_,
        buffer_,
        *parser_,
        beast::bind_front_handler(
            &http_session::on_read,
            shared_from_this()));
}

void
http_session::
on_read(beast::error_code ec, std::size_t)
{
    // This means they closed the connection
    if(ec == http::error::end_of_stream)
    {
        stream_.socket().shutdown(tcp::socket::shutdown_send, ec);
        return;
    }

    // Handle the error, if any
    if(ec)
        return fail(ec, "read");

    // See if it is a WebSocket Upgrade
    if(websocket::is_upgrade(parser_->get()))
    {
        // Create a websocket session, transferring ownership
        // of both the socket and the HTTP request.
        boost::make_shared<websocket_session>(
            stream_.release_socket(),
                state_)->run(parser_->release());
        return;
    }

    // Handle request
    http::message_generator msg = handle_request(state_, parser_->release());
    // http::message_generator msg = handle_request(state_->doc_root(), parser_->release());

    // Determine if we should close the connection
    bool keep_alive = msg.keep_alive();

    auto self = shared_from_this();

    // Send the response
    beast::async_write(
        stream_, std::move(msg),
        [self, keep_alive](beast::error_code ec, std::size_t bytes)
        {
            self->on_write(ec, bytes, keep_alive);
        });
}

void
http_session::
on_write(beast::error_code ec, std::size_t, bool keep_alive)
{
    // Handle the error, if any
    if(ec)
        return fail(ec, "write");

    if(! keep_alive)
    {
        // This means we should close the connection, usually because
        // the response indicated the "Connection: close" semantic.
        stream_.socket().shutdown(tcp::socket::shutdown_send, ec);
        return;
    }

    // Read another request
    do_read();
}
