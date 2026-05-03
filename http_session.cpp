//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/vinniefalco/CppCon2018
//

#include "FuzeHttp.hpp"
#include "field_lengths.h"
#include "http_session.hpp"
#include "shared_state.hpp"
#include "websocket_session.hpp"
#include <boost/algorithm/string/replace.hpp>
#include <boost/beast/http/status.hpp>
#include <boost/beast/http/string_body_fwd.hpp>
#include <boost/config.hpp>
#include <boost/filesystem.hpp>
#include <boost/json/serialize.hpp>
#include <boost/locale.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <Magick++.h>
#include <charconv>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

http_session::http_session(boost::asio::ip::tcp::socket&& socket, shared_state* state, FuzeHttp::Controller<shared_state*>* controller)
		: stream_(std::move(socket)),
		state_(state),
		controller(controller) {
}

//------------------------------------------------------------------------------

const std::string forbidden_file_name_chars = "#?";

const std::set<std::string, std::less<>> image_formats = {"bmp", "gif", "ico", "jpg", "jpeg", "jxl", "png", "svg", "webp"};
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

// Append an HTTP rel-path to a local filesystem path.
// The returned path is normalized for the platform.
std::string path_cat(
	beast::string_view base,
	beast::string_view path) {
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
http::message_generator handle_request(
		shared_state* state,
		FuzeHttp::Controller<shared_state*>* controller,
		http::request<http::string_body, http::basic_fields<std::allocator<char>>>&& req) {
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
		res.body() = "A server error occurred: '" + std::string(what) + "'";
		res.prepare_payload();
		return res;
	};

	std::string decoded_url = FuzeHttp::getDecodedURL(req.target());
	std::string_view path_name = FuzeHttp::getPathName(decoded_url);

	// Matches paths in urls.cpp
	FuzeHttp::Response basic_res;
	try {
		basic_res = controller->matchPathAndExecute(state, req);
		std::cout << "[http_session] basic_res.status: " << basic_res.status << std::endl;
		if (basic_res.status != http::status::not_found) {
			if (basic_res.json || basic_res.body)
				return FuzeHttp::buildResponse<http::string_body>(basic_res, req);
			else if (basic_res.file)
				return FuzeHttp::buildResponse<http::file_body>(basic_res, req);
			else
				return FuzeHttp::buildResponse<http::empty_body>(basic_res, req);
		}
	}
	catch(const std::exception& e) {
		std::string error_text = std::format("[http_session] {}", e.what());
		std::cerr << error_text << std::endl;
		return server_error(error_text);
	}


	auto const getNumberFromPath = [&path_name](int start_index) {
		std::size_t found = path_name.find_first_not_of("0123456789", start_index+1);

		if (found == start_index) {
			throw (std::string("Invalid group ID; cannot be empty."));
		}
		else if (path_name[start_index] == '/') {
			throw (std::string("First character cannot be a /. Try adding +1."));
		}
		else if (path_name[found] != '/') {
			throw (std::string("Invalid group ID; trailing '/' not found."));
		}
		else {
			int number_in_url;
			// Get number ID from URL substring
			std::cout << "Substring: " << path_name.substr(start_index, found - start_index) << std::endl;
			std::from_chars(path_name.substr(start_index, found - start_index).data(), path_name.substr(start_index, found - start_index).data() + path_name.substr(start_index, found - start_index).size(), number_in_url);
			return std::make_pair(number_in_url, found);
		}
		throw ("Program should not reach here.");
		return std::make_pair(-1, found);
	};

	/*
	auto const getUserFromToken	= [&req, &state]() {
		std::string token;
		// boost::intrusive::list_iterator<boost::intrusive::bhtraits<boost::beast::http::basic_fields<std::allocator<char>>::element, boost::intrusive::list_node_traits<void*>, boost::intrusive::normal_link, boost::intrusive::dft_tag, 1>, true> it = req.begin();
		// Iterates value_type. See: https://www.boost.org/doc/libs/boost_1_82_0/libs/beast/doc/html/beast/ref/boost__beast__http__basic_fields__value_type.html
		// std::cout << "[http_session] getUserFromToken headers" << std::endl;
		for (auto it = req.begin(); it != req.end(); it++) {
			// std::cout << it->name_string() << ": " << it->value() << std::endl;
			if (it->name_string() == "token") {
				token = it->value();
				std::cout << "Found token in header. It is " << token << std::endl;
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
				client_id = static_cast<int>(User::PUBLIC);
				return std::make_pair(client_id, key);
			}
			else {
				if (state->userExists(username)) {
					client_id = state->getIdFromUsername(username);
					// if (state->checkUserKey(client_id, key))
					//	return std::make_pair(client_id, key);
					// else
						throw (std::string("Key does not match user."));
				}
				else
					throw (std::string("User ") + username + " not found.");
			}
		}
		// client id -1 means there was an error
		return std::make_pair(-1, key);
	};
	*/
	if (req.method() == http::verb::post) {
		if (req.target() == "/api/upload/") {
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
			std::ofstream outfile(std::format("{}/{}", state->getMediaLocation().string(), out_filename), std::ios::binary);
			bool is_initial_line = true;
			bool previous_line_ends_with_carriage_return = false;
			while (std::getline(req_stream, req_line)) {
				// std::cout << req_line << std::endl;
				// std::cout << req_line.length() << ", " << req_terminator.length() << std::endl;
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
					thumbnail.read(std::format("{}/{}", state->getMediaLocation().string(), out_filename));
					thumbnail.strip(); // Removes metadata
					thumbnail.resize("150x150");
					thumbnail.quality(50);
					thumbnail.write(std::format("{}/thumbnails/THUMBNAIL_{}.{}", state->getMediaLocation().string(), out_filename, state->getThumbnailFileFormat()));
				}
				catch( Magick::Warning& magick_warning ) {
					std::cerr << "[Magick++] WARNING: " << magick_warning.what() << std::endl << "Thumbnail might not be made." << std::endl;
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
		/*
		else if (path_name.substr(0, 5) == "/api/") {
			std::pair<int, std::string> client;
			try {
				client = getUserFromToken();
			}
			catch(std::string error_text) {
				return api_response(http::status::bad_request, error_text);
			}
			if (path_name == "/api/message/" || path_name == "/api/thread/") {
				http::response<http::empty_body> res;
				json request_json = json::parse(req.body());
				bool is_thread;
				json post_json;
				if (path_name == "/api/thread/") {
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
						return api_response(http::status::bad_request, std::string("More than 4 files attatched."));
					}
					std::string message_content = post_json["content"].template get<std::string>();
					if ((message_content.length() == 0 && post_json["files"].size() == 0) || message_content.length() > POST_MAX_CONTENT)
						return api_response(http::status::bad_request, std::string("The post does not meet the constraints set by the server.\nThis could mean that the message content was empty and no files were uploaded, or the message content is too long."));
					if (is_thread) {
						if (!state->userHasPermission(client.first, PERMISSION::CREATE_THREAD))
							return api_response(http::status::forbidden, std::string("User lacks permission CREATE_THREAD."));
						request_json["thread"]["post_zero"]["key"] = client.second; // key is to identify the author of a post
						int new_thread_id = state->main_board()->createThread(request_json["thread"]);
						res.set("New-Thread-Id", std::to_string(new_thread_id));
						res.result(201);
					}
					else {
						int new_message_thread_id = post_json["thread_id"].template get<int>();
						if (state->main_board()->threadExists(new_message_thread_id)) {
							if (!state->main_board()->getThread(new_message_thread_id)->userHasPermission(client.first, PERMISSION::SEND_MESSAGE))
								return api_response(http::status::forbidden, std::string("User lacks permission SEND_MESSAGE within this thread."));
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
					res.set("message", "One or more JSON fields missing in post.");
					res.result(400);
				}
				res.prepare_payload();
				return res;
			}
			// The URL extends past /thread/, used for permission management
			else if (path_name.substr(0, 12) == "/api/thread/") {
				std::pair<int, int> thread_in_url;
				try {
					thread_in_url = getNumberFromPath(12);
				}
				catch(std::string error_text) {
					return api_response(http::status::bad_request, std::string("Bad URL, couldn't get thread ID."));
				}

				boost::shared_ptr<Thread> thread = state->getThread(0, thread_in_url.first);
				if (path_name.substr(thread_in_url.second, 19) == "/permissions/group/") {
					int group_id;
					try {
						group_id = getNumberFromPath(thread_in_url.second+19).first;
					}
					catch(std::string error_text) {
						return api_response(http::status::bad_request, std::string("Bad URL, couldn't get group ID."));
					}
					if (!thread->userHasPermissionForGroup(client.first, PERMISSION::MANAGE_PERMISSIONS, group_id))
						return api_response(http::status::forbidden, std::string("User lacks permission MANAGE_PERMISSIONS"));
					else if (thread->permissionCollectionExistsForGroup(group_id))
						return api_response(http::status::bad_request, std::string("Permissions for this group are already set."));
					else {
						state->main_board()->addGroupPermissionCollectionToThread(group_id, thread_in_url.first);
						// thread->addGroupPermissionCollection(group_id); // Not used because pointer is read-only
						return api_response(http::status::ok, std::string("Group permission collection created"));
					}
				}
				else if (path_name.substr(thread_in_url.second, 18) == "/permissions/user/") {
					int user_id;
					try {
						user_id = getNumberFromPath(thread_in_url.second+18).first;
					}
					catch(std::string error_text) {
						return api_response(http::status::bad_request, std::string("Bad URL, couldn't get user ID."));
					}
					if (!thread->userHasPermissionForUser(client.first, PERMISSION::MANAGE_PERMISSIONS, user_id))
						return api_response(http::status::forbidden, std::string("User lacks permission MANAGE_PERMISSIONS"));
					else if (thread->permissionCollectionExistsForUser(user_id))
						return api_response(http::status::bad_request, std::string("Permissions for this user are already set."));
					else {
						state->main_board()->addUserPermissionCollectionToThread(user_id, thread_in_url.first);
						return api_response(http::status::ok, std::string("User permission collection created"));
					}
				}
				else
					return api_response(http::status::not_found, std::string("/api/server sub-URL not found"));
			}
			else if (req.target() == "/api/create_group/") {
				if (!state->userHasPermission(client.first, PERMISSION::MANAGE_PERMISSIONS))
					return api_response(http::status::forbidden, std::string("Client lacks permission MANAGE_PERMISSIONS"));
				else if (state->getUserRank(client.first) >= state->getOrderedGroups()->size() - 2)
					return api_response(http::status::forbidden, std::string("Only users within a group with rank above \"User\" can create groups."));
				std::string new_group_name;
				try {
					nlohmann::json request_json = json::parse(req.body());
					if (	request_json.contains("group")
						&& request_json["group"].contains("name")
					) {
						new_group_name = request_json["group"]["name"].template get<std::string>();
					}
					else {
						return api_response(http::status::bad_request, "One or more JSON fields missing in group.");
					}
				}
				catch (const json::exception& exception) {
					return api_response(http::status::bad_request, exception.what());
				}
				int new_group_rank = state->getUserRank(client.first) + 1;
				state->addGroup(new_group_name, new_group_rank);
				return api_response(http::status::ok, std::string("Group created"));
			}
			// For now assume the URL ends with add_groups/
			else if (path_name.substr(0, 10) == "/api/user/") {
				int user_in_url;
				try {
					user_in_url = getNumberFromPath(10).first;
				}
				catch(std::string error_text) {
					return api_response(http::status::bad_request, std::string("Bad URL."));
				}
				json request_json;
				std::vector<int> groups_to_add;
				try {
					request_json = json::parse(req.body());
					groups_to_add = request_json["groups_by_id"].template get<std::vector<int>>();
				}
				catch (const json::exception& exception) {
					return api_response(http::status::bad_request, exception.what());
				}

				BasicResponse function_response = state->addUserToGroups(client.first, user_in_url, groups_to_add);
				return api_response(function_response.status, function_response.message);
			}
			else if (path_name.substr(0, 12) == "/api/server/") {
				std::cout << path_name.substr(12, 18) << std::endl;
				if (path_name.substr(12, 18) == "permissions/group/") {
					int group_in_url;
					try {
						group_in_url = getNumberFromPath(12+18).first;
					}
					catch(std::string error_text) {
						return api_response(http::status::bad_request, std::string("Bad URL."));
					}
					if (state->userHasPermissionForGroup(client.first, PERMISSION::MANAGE_PERMISSIONS, group_in_url)) {
						state->addGroupPermissionCollection(group_in_url);
						return api_response(http::status::ok, std::string("Group permission collection created"));
					}
					else
						return api_response(http::status::forbidden, std::string("Client lacks permission MANAGE_PERMISSIONS for this group"));
				}
				else if (path_name.substr(12, 17) == "permissions/user/") {
					int user_in_url;
					try {
						user_in_url = getNumberFromPath(12+17).first;
					}
					catch(std::string error_text) {
						return api_response(http::status::bad_request, std::string("Bad URL."));
					}
					if (state->userHasPermissionForUser(client.first, PERMISSION::MANAGE_PERMISSIONS, user_in_url)) {
						state->addUserPermissionCollection(user_in_url);
						return api_response(http::status::ok, std::string("User permission collection created"));
					}
					else
						return api_response(http::status::forbidden, std::string("Client lacks permission MANAGE_PERMISSIONS for this user"));
				}
				else
					return api_response(http::status::not_found, std::string("/api/server sub-URL not found"));
			}
			else if (decoded_url == "/api/change_password/") {
				json request_json;
				try {
					request_json = json::parse(req.body());
				}
				catch (const json::exception& exception) {
					return api_response(http::status::bad_request, exception.what());
				}
				if (request_json.contains("old_password") && request_json.contains("new_password")) {
					std::string old_password = request_json["old_password"].template get<std::string>();
					std::string new_password = request_json["new_password"].template get<std::string>();
					char change_password_result = db_change_password(client.first, old_password.c_str(), new_password.c_str());
					if (change_password_result == 't')
						return api_response(http::status::ok, std::string("Account password changed successfully.."));
					else if (change_password_result == '\0')
						return api_response(http::status::bad_request, std::string("No account with this username exists."));
					else
						return api_response(http::status::bad_request, "Password change operation failed.");
				}
				else
					return api_response(http::status::bad_request, "One or more JSON fields missing.");
			}
			else
				return api_response(http::status::not_found, std::string("/api/ sub-URL not found."));
		}
		*/
		else {
			std::cout << "Unknown target: " << req.target() << std::endl;
			std::cout << "Unknown target: " << decoded_url << std::endl;
			http::response<http::empty_body> res;
			res.result(404);
			res.prepare_payload();
			return res;
		}
	}
	/*
	else if (req.method() == http::verb::put) {

		std::pair<int, std::string> client;
		try {
			client = getUserFromToken();
		}
		catch(std::string error_text) {
			return api_response(http::status::bad_request, error_text);
		}
		if (req.target() == "/api/group_heirarchy/") {
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
				BasicResponse function_response = state->setGroupHeirarchy(client.first, new_group_heirarchy);
				return api_response(function_response.status, function_response.message);
			}
			else
				return api_response(http::status::bad_request, "ordered_groups not found in JSON request");
		}
		else if (path_name.substr(0, 24) == "/api/server/permissions/") {
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
			if (_permission_number < 0 || _permission_number >= static_cast<int>(PERMISSION::NUMBER_OF_PERMISSIONS))
				return api_response(http::status::bad_request, std::string("Invalid permission number in JSON"));
			PERMISSION permission = static_cast<PERMISSION>(_permission_number);
			if (_permission_setting < 0 || _permission_setting >= 3)
				return api_response(http::status::bad_request, std::string("Invalid permission setting in JSON"));
			THREE_STATE_SETTING permission_setting = static_cast<THREE_STATE_SETTING>(_permission_setting);

			if (path_name.substr(24, 6) == "group/") {
				int group_id;
				try {
					group_id = getNumberFromPath(24+6).first;
				}
				catch(std::string error_text) {
					return api_response(http::status::bad_request, std::string("Bad URL."));
				}
				if (state->userHasPermissionForGroup(client.first, PERMISSION::MANAGE_PERMISSIONS, group_id)) {
					state->setGroupPermission(group_id, permission, permission_setting);
					return api_response(http::status::ok, std::string("Group permission updated"));
				}
				else
					return api_response(http::status::forbidden, std::string("Permission denied for this client"));
			}
			else if (path_name.substr(24, 5) == "user/") {
				int user_id;
				try {
					user_id = getNumberFromPath(24+5).first;
				}
				catch(std::string error_text) {
					return api_response(http::status::bad_request, std::string("Bad URL."));
				}
				if (state->userHasPermissionForUser(client.first, PERMISSION::MANAGE_PERMISSIONS, user_id)) {
					state->setUserPermission(user_id, permission, permission_setting);
					return api_response(http::status::ok, std::string("User permission updated"));
				}
				else
					return api_response(http::status::forbidden, std::string("Permission denied for this client"));
			}
			else
				return api_response(http::status::not_found, std::string("/api/server/permissions sub-URL not found"));
		}
		else if (path_name.substr(0, 12) == "/api/thread/") {
			// Assume we edit permissions, because that is the only feature implemented for PUT /api/thread/
			std::pair<int, int> thread_in_url;
			try {
				thread_in_url = getNumberFromPath(12);
			}
			catch(std::string error_text) {
				return api_response(http::status::bad_request, std::string("Bad URL."));
			}
			if (!state->main_board()->threadExists(thread_in_url.first))
				return api_response(http::status::not_found, std::string("Thread with ID ") + std::to_string(thread_in_url.first) + "was not found");
			boost::shared_ptr<Thread> thread = state->getThread(0, thread_in_url.first);

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
			if (_permission_number < 0 || _permission_number >= static_cast<int>(PERMISSION::NUMBER_OF_PERMISSIONS))
				return api_response(http::status::bad_request, std::string("Invalid permission number in JSON"));
			PERMISSION permission = static_cast<PERMISSION>(_permission_number);
			if (_permission_setting < 0 || _permission_setting >= 3)
				return api_response(http::status::bad_request, std::string("Invalid permission setting in JSON"));
			THREE_STATE_SETTING permission_setting = static_cast<THREE_STATE_SETTING>(_permission_setting);

			if (path_name.substr(thread_in_url.second, 19) == "/permissions/group/") {
				std::pair<int, int> group_in_url;
				try {
					group_in_url = getNumberFromPath(thread_in_url.second+19);
				}
				catch(std::string error_text) {
					return api_response(http::status::bad_request, std::string("Bad URL."));
				}
				if (thread->userHasPermissionForGroup(client.first, PERMISSION::MANAGE_PERMISSIONS, group_in_url.first)) {
					state->main_board()->setGroupPermissionForThread(group_in_url.first, permission, permission_setting, thread_in_url.first);
					return api_response(http::status::ok, std::string("Group permission updated"));
				}
				else
					return api_response(http::status::forbidden, std::string("Permission denied for this client"));
			}
			else if (path_name.substr(thread_in_url.second, 18) == "/permissions/user/") {
				std::pair<int, int> user_in_url;
				try {
					user_in_url = getNumberFromPath(thread_in_url.second+18);
				}
				catch(std::string error_text) {
					return api_response(http::status::bad_request, std::string("Bad URL."));
				}
				if (thread->userHasPermissionForUser(client.first, PERMISSION::MANAGE_PERMISSIONS, user_in_url.first)) {
					state->main_board()->setUserPermissionForThread(user_in_url.first, permission, permission_setting, thread_in_url.first);
					return api_response(http::status::ok, std::string("User permission updated"));
				}
				else
					return api_response(http::status::forbidden, std::string("Permission denied for this client"));
			}
			else
				return api_response(http::status::not_found, std::string("/api/thread/ sub-URL not found"));
		}
		else
			return not_found(req.target());
	}
	*/
	/*
	else if (req.method() == http::verb::delete_) {
		if (path_name.substr(0, 10) == "/api/post/") {
			int thread_id, message_id;
			std::pair<int, int> thread_in_url;
			try {
				thread_in_url = getNumberFromPath(10);
			}
			catch(std::string error_text) {
				return api_response(http::status::bad_request, std::string("Bad URL."));
			}
			thread_id = thread_in_url.first;
			try {
				message_id = getNumberFromPath(thread_in_url.second+1).first;
			}
			catch(std::string error_text) {
				return api_response(http::status::bad_request, std::string("Bad URL."));
			}
			if (!state->main_board()->threadExists(thread_id))
				return api_response(http::status::bad_request, std::string(std::string("Thread ") + std::to_string(thread_id) + " does not exist"));
			std::pair<int, std::string> client;
			try {
				client = getUserFromToken();
			}
			catch(std::string error_text) {
				return api_response(http::status::bad_request, error_text);
			}
			if (message_id == 0) { // Is a thread
				if (state->main_board()->getThread(thread_id)->userHasPermission(client.first, PERMISSION::DELETE_POST)) {
					state->main_board()->deleteThread(thread_id);
					return api_response(http::status::ok, std::string("Deleting thread #") + std::to_string(thread_id));
				}
				else
					return api_response(http::status::forbidden, std::string("User does not have permission to delete this thread."));
			}
			else {
				if (!state->main_board()->messageExistsInThread(message_id, thread_id)) {
					return api_response(http::status::bad_request, std::string("Message ") + std::to_string(message_id) + " does not exist in thread " + std::to_string( thread_id));
				}
				if (state->main_board()->getThread(thread_id)->userHasPermission(client.first, PERMISSION::DELETE_POST) || state->main_board()->keyMatchesMessageInThread(client.second.c_str(), message_id, thread_id)) {
					state->main_board()->deleteMessageFromThread(message_id, thread_id);
					return api_response(http::status::ok, std::string("Deleting message #") + std::to_string(thread_id) + "/" + std::to_string(message_id));
				}
				else
					return api_response(http::status::bad_request, std::string("Permission denied for message deletion."));
			}
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
			std::cout << "Group in URL: " << group_in_url.first << ", path_name.length(): " << path_name.length() << ", group.second: " << group_in_url.second << std::endl;
			if (state->userHasPermissionForGroup(client.first, PERMISSION::MANAGE_PERMISSIONS, group_in_url.first)) {
				if (path_name.length() > group_in_url.second+1) {
					std::cout << path_name.substr(group_in_url.second+1, 7) << std::endl;
					if (path_name.substr(group_in_url.second+1, 7) == "member/") {
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
		else if (path_name.substr(0, 12) == "/api/server/") {
			std::cout << path_name.substr(12, 18) << std::endl;
			if (path_name.substr(12, 18) == "permissions/group/") {
				// TODO check if group exists
				std::pair<int, std::string> client;
				try {
					client = getUserFromToken();
				}
				catch(std::string error_text) {
					return api_response(http::status::bad_request, error_text);
				}
				std::pair<int, int> group_in_url = getNumberFromPath(30);
				if (state->userHasPermissionForGroup(client.first, PERMISSION::MANAGE_PERMISSIONS, group_in_url.first)) {
					state->removeGroupPermissionCollection(group_in_url.first);
					return api_response(http::status::ok, std::string("Group permission collection removed"));
				}
				else
					return api_response(http::status::unauthorized, std::string("Couldn't remove group permission collection; Permission denied for this client"));
				// return api_response(http::status::not_implemented, std::string("Group ID: " + std::to_string(group_in_url.first) + " but action not implemented"));
			}
			else if (path_name.substr(12, 17) == "permissions/user/") {
				std::pair<int, std::string> client;
				try {
					client = getUserFromToken();
				}
				catch(std::string error_text) {
					return api_response(http::status::bad_request, error_text);
				}
				int user_in_url = getNumberFromPath(29).first;
				// TODO check if user exists
				// TODO get user from token and check his/her permission
				// if (shared_state->userHasPermission()
				if (state->userHasPermissionForUser(client.first, PERMISSION::MANAGE_PERMISSIONS, user_in_url)) {
					state->removeUserPermissionCollection(user_in_url);
					return api_response(http::status::ok, std::string("User permission collection removed"));
				}
				else
					return api_response(http::status::unauthorized, std::string("Couldn't remove group permission collection; Permission denied for this client"));
			}
			else
				return api_response(http::status::not_found, std::string("/api/server sub-URL not found"));
		}
		else if (path_name.substr(0, 12) == "/api/thread/") {
			std::pair<int, std::string> client;
			try {
				client = getUserFromToken();
			}
			catch(std::string error_text) {
				return api_response(http::status::bad_request, error_text);
			}
			// Assume we edit permissions, because that is the only feature implemented for PUT /api/thread/
			std::pair<int, int> thread_in_url;
			try {
				thread_in_url = getNumberFromPath(12);
			}
			catch(std::string error_text) {
				return api_response(http::status::bad_request, std::string("Bad URL."));
			}
			boost::shared_ptr<Thread> thread = state->getThread(0, thread_in_url.first);

			if (path_name.substr(thread_in_url.second, 19) == "/permissions/group/") {
				int group_id;
				try {
					group_id = getNumberFromPath(thread_in_url.second+19).first;
				}
				catch(std::string error_text) {
					return api_response(http::status::bad_request, std::string("Bad URL."));
				}
				if (thread->userHasPermissionForGroup(client.first, PERMISSION::MANAGE_PERMISSIONS, group_id)) {
					state->main_board()->removeGroupPermissionCollectionFromThread(group_id, thread_in_url.first);
					return api_response(http::status::ok, std::string("Group permission deleted"));
				}
				else
					return api_response(http::status::forbidden, std::string("Permission denied for this client"));
			}
			else if (path_name.substr(thread_in_url.second, 18) == "/permissions/user/") {
				int user_id;
				try {
					user_id = getNumberFromPath(thread_in_url.second+18).first;
				}
				catch(std::string error_text) {
					return api_response(http::status::bad_request, std::string("Bad URL."));
				}
				if (thread->userHasPermissionForUser(client.first, PERMISSION::MANAGE_PERMISSIONS, user_id)) {
					state->main_board()->removeUserPermissionCollectionFromThread(user_id, thread_in_url.first);
					return api_response(http::status::ok, std::string("User permission deleted"));
				}
				else
					return api_response(http::status::forbidden, std::string("Permission denied for this client"));
			}
			else
				return api_response(http::status::not_found, std::string("/api/thread/ sub-URL not found"));
		}
		else {
			return api_response(http::status::bad_request, std::string("Unknown target: ") + std::string(req.target()));
		}
	}
	*/
	/*
	else if(req.method() == http::verb::head) {
		http::response<http::empty_body> res{http::status::ok, req.version()};
		res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
		res.set(http::field::content_type, mime_type(path));
		res.content_length(size);
		res.keep_alive(req.keep_alive());
		return res;
	}
	*/
	else {
		http::response<http::empty_body> res{http::status::not_implemented, req.version()};
		res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
		res.keep_alive(req.keep_alive());
		res.prepare_payload();
		return res;
	}
}

//------------------------------------------------------------------------------

void http_session::run() {
	do_read();
}

// Report a failure
void http_session::fail(beast::error_code ec, char const* what) {
	// Don't report on canceled operations
	if(ec == boost::asio::error::operation_aborted)
		return;

	std::cerr << what << ": " << ec.message() << "\n";
}

void http_session::do_read() {
	// Construct a new parser for each message
	parser_.emplace();

	// Apply a reasonable limit to the allowed size
	// of the body in bytes to prevent abuse.
	// 6MB would match 4chins
	// This is 100MB
	parser_->body_limit(100 << 20);

	// Set the timeout.
	stream_.expires_after(std::chrono::minutes(60));

	// Read a request
	http::async_read(
		stream_,
		buffer_,
		*parser_,
		beast::bind_front_handler(
			&http_session::on_read,
			shared_from_this()));
}

void http_session::on_read(beast::error_code ec, std::size_t) {
	// This means they closed the connection
	if(ec == http::error::end_of_stream) {
		stream_.socket().shutdown(boost::asio::ip::tcp::socket::shutdown_send, ec);
		return;
	}

	// Handle the error, if any
	if(ec)
		return fail(ec, "read");

	// See if it is a WebSocket Upgrade
	if(websocket::is_upgrade(parser_->get())) {
		// Create a websocket session, transferring ownership
		// of both the socket and the HTTP request.
		boost::make_shared<websocket_session>(stream_.release_socket(), state_)->run(parser_->release());
		return;
	}

	// Handle request
	http::message_generator msg = handle_request(state_, controller, parser_->release());
	// http::message_generator msg = handle_request(state_->doc_root(), parser_->release());

	// Determine if we should close the connection
	bool keep_alive = msg.keep_alive();

	auto self = shared_from_this();

	// Send the response
	beast::async_write(
		stream_, std::move(msg),
		[self, keep_alive](beast::error_code ec, std::size_t bytes) {
			self->on_write(ec, bytes, keep_alive);
		}
	);
}

void http_session::on_write(beast::error_code ec, std::size_t, bool keep_alive) {
	// Handle the error, if any
	if(ec)
		return fail(ec, "write");

	if(! keep_alive) 	{
		// This means we should close the connection, usually because
		// the response indicated the "Connection: close" semantic.
		stream_.socket().shutdown(boost::asio::ip::tcp::socket::shutdown_send, ec);
		return;
	}

	// Read another request
	do_read();
}
