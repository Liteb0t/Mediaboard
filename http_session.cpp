//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/vinniefalco/CppCon2018
//

#include "http_session.hpp"
#include "websocket_session.hpp"
#include <boost/config.hpp>
#include <boost/locale.hpp>
#include <boost/url/src.hpp>
#include <boost/uuid/uuid.hpp>
// #include <boost/lexical_cast.hpp>
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

const std::set<std::string, std::less<>> image_formats = {"gif", "jpg", "jpeg", "jxl", "png", "webp"};
const bool fileIsImage(std::string* file_name) {
	int dot_index = file_name->rfind('.');
	if (dot_index != std::string::npos) {
		std::string_view file_extension = file_name->substr(dot_index+1);
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
    auto const bad_request =
    [&req](beast::string_view why)
    {
        http::response<http::string_body> res{http::status::bad_request, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        res.body() = std::string(why);
        res.prepare_payload();
        return res;
    };

    // Returns a not found response
    auto const not_found =
    [&req](beast::string_view target)
    {
        http::response<http::string_body> res{http::status::not_found, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        res.body() = "The resource '" + std::string(target) + "' was not found.";
        res.prepare_payload();
        return res;
    };

    // Returns a server error response
    auto const server_error =
    [&req](beast::string_view what)
    {
        http::response<http::string_body> res{http::status::internal_server_error, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        res.body() = "An error occurred: '" + std::string(what) + "'";
        res.prepare_payload();
        return res;
    };

    // Request path must be absolute and not contain "..".
    if( req.target().empty() ||
        req.target()[0] != '/' ||
        req.target().find("..") != beast::string_view::npos)
        return bad_request("Illegal request-target");

	std::cout << "req target: " << req.target() << "\n";
	
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

	// boost::system::result<boost::urls::url_view> url_parse_result = boost::urls::parse_uri(req.target());
	// boost::urls::url_view parsed_url = url_parse_result.value();
	// boost::urls::url_view parsed_url(req.target());
	// std::cout << "Parsed path: " << parsed_url.path() << std::endl;

		// Make sure we can handle the method
   	if( req.method() == http::verb::get ||
   	    req.method() == http::verb::head)
	{
		bool is_media = false;
        // return bad_request("Unknown HTTP-method");
    	// Build the path to the requested file
    	std::string path;
		if (req.target().substr(0, 6) == "/media") {
			is_media = true;
			path = path_cat(state->doc_root(), decoded_url.substr(6));
		}
		else if (req.target().substr(0, 5) == "/api/") {
			http::response<http::string_body> res;
			res.set(http::field::content_type, "application/json");
			// std::cout << req.target().substr(5, 13) << std::endl;
			if (req.target().substr(5, 12) == "get_threads/") {
				res.result(http::status::ok);
				// json response_json;
				// res.body() = json::serialize(json_response);
				res.body() = state->main_board.dumpAllThreads();
			}
			else if (req.target().substr(5, 8) == "threads/") {
				std::size_t found = req.target().find_first_not_of("0123456789", 13);
				if (req.target()[found] != '/') {
					std::cout << "Error: invalid thread ID" << std::endl;
					res.result(500);
				}
				else if (found == 13) {
					std::cout << "Error: thread ID cannot be empty" << std::endl;
					res.result(500);
				}
				else {
					int thread_in_url;
					std::from_chars(req.target().substr(13, found).data(), req.target().substr(13, found).data() + req.target().substr(13, found).size(), thread_in_url);
					// int thread_in_url = atoi(req.target().substr(13, found).c_str());
					if (state->main_board.threadExists(thread_in_url)) {
						std::cout << "[http_session] HEADERS:" << std::endl;
						std::string key;

						// boost::intrusive::list_iterator<boost::intrusive::bhtraits<boost::beast::http::basic_fields<std::allocator<char>>::element, boost::intrusive::list_node_traits<void*>, boost::intrusive::normal_link, boost::intrusive::dft_tag, 1>, true> it = req.begin();
						 // Iterates value_type. See: https://www.boost.org/doc/libs/boost_1_82_0/libs/beast/doc/html/beast/ref/boost__beast__http__basic_fields__value_type.html
						for (auto it = req.begin(); it != req.end(); it++) {
							std::cout << it->name_string() << ": " << it->value() << "\n";
							if (it->name_string() == "key") {
								key = it->value();
								std::cout << "Found key in header. It is " << key << std::endl;
								break;
							}
						}
						res.body() = state->main_board.dumpPostsInThread(thread_in_url, key);
						res.result(http::status::ok);
					}
					else {
						std::cout << "Error: thread '" << thread_in_url << "' does not exist" << std::endl;
						res.result(404);
					}
				}
			}
			else {
				res.result(404);
				res.body() = "ERROOOORRRRRR!!!! OH NOES!!!";
			}
			res.prepare_payload();
			return res;
		}
		else if (req.target().back() == '/') {
			path = "index.html";
			std::cout << "/path: " << path << std::endl;
		}
		else {
			// This is used to access files in the server's directory
			// path = path_cat(state->doc_root(), req.target());
			// path = parsed_url.path().substr(1);
			path = decoded_url.substr(1);
		}

    	// Attempt to open the file
    	beast::error_code ec;
    	http::file_body::value_type body;
		std::cout << "Opening path: " << path << std::endl;
    	body.open(path.c_str(), beast::file_mode::scan, ec);

    	// Handle the case where the file doesn't exist
    	if(ec == boost::system::errc::no_such_file_or_directory)
    	    return not_found(req.target());

    	// Handle an unknown error
    	if(ec)
			return server_error(ec.message());

		std::string filename;
		if (is_media) {
			int filename_start_index = req.target().rfind("/") + 1;
			filename = req.target().substr(filename_start_index, req.target().length() - filename_start_index);
			int filename_extension_index;
			if ((filename_extension_index = filename.rfind(".")) == -1) {
				filename_extension_index = filename.size();
			}
			filename.erase(filename_extension_index - 36, 36);
			std::cout << "Is media. Filename: " << filename << std::endl;
		}
		else
			is_media = false;

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
		if (is_media) {
			res.set("Content-Disposition", "attachment; filename=\"" + filename + "\"");
		}
    	res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    	res.set(http::field::content_type, mime_type(path));
    	res.content_length(size);
    	res.keep_alive(req.keep_alive());
    	return res;
	}
	else if (req.method() == http::verb::post) {
		if (req.target() == "/api/create_thread/") {
			http::response<http::string_body> res;
			json request_json = json::parse(req.body());
			if (request_json.contains("thread") && 
					// request_json["thread"].contains("key") &&
					request_json["thread"].contains("post_zero") &&
					request_json["thread"]["post_zero"].contains("files") && 
					request_json["thread"]["post_zero"].contains("name") && 
					request_json["thread"]["post_zero"].contains("content")) {
				if (request_json["thread"]["post_zero"]["files"].size() > 4) {
					std::cerr << "Denied: More than 4 files in thread\n";
					res.result(500);
				}
				else {
					state->main_board.createThread(request_json["thread"]);
					res.result(201);
				}
			}
			else {
				res.result(400);
			}
			res.prepare_payload();
			return res;
		}
		else if (req.target() == "/api/create_message/") {
			http::response<http::string_body> res;
			json request_json = json::parse(req.body());
			if (request_json.contains("post") &&
					request_json["post"].contains("files") && 
					request_json["post"].contains("name") && 
					request_json["post"].contains("content") && 
					request_json["post"].contains("key")) {
				if (request_json["post"]["files"].size() > 4) {
					std::cerr << "Denied: More than 4 files in message\n";
					res.result(500);
				}
				else {
					int new_message_thread_id = request_json["post"]["thread_id"].template get<int>();
					std::string new_message_key = request_json["post"]["key"].template get<std::string>();
					if (state->main_board.threadExists(new_message_thread_id)) {
						int new_message_id = state->main_board.createPost(request_json["post"]);
						std::string new_message_dump = state->main_board.dumpPost(new_message_thread_id, new_message_id, new_message_key);
						state->sendToThread(new_message_dump, new_message_thread_id);
						res.result(201);
					}
					else {
						std::cerr << "Couldn't create message because the thread with ID " << new_message_thread_id << " does not exist" << std::endl;
						res.result(400);
					}
				}
			}
			else {
				res.result(400);
			}
			res.prepare_payload();
			return res;
		}
		else if (req.target() == "/api/upload/") {
			// request_parser<empty_body> req_parser;
			// std::string content_dispo =  req.get()[http::field::content_disposition] << std::endl;
			// std::cout << req.body() << std::endl;
			// std::string req_string = req.body();
			// std::cout << req_string << std::endl;
			std::istringstream req_stream(req.body());
			std::string req_line;
			int i = 0;
			std::getline(req_stream, req_line);
			std::string req_terminator = req_line.substr(0, req_line.length()-1) + "--\r";
			// std::cout << "Request ID: " << req_terminator << std::endl;
			std::string out_filename;
			bool empty_line = false;
			while (!empty_line) {
				std::getline(req_stream, req_line, '\n');
				if (req_line == "\r") {
					empty_line = true;
					// std::cout << "CR FOUND";
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
			sanitiseFileName(&out_filename);
			std::cout << "Sanitised out_filename: " << out_filename << std::endl;
			// std::cout << "START OF FILE" << std::endl;
			// std::string out_filename_bez_extension;

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
			bool terminator_found = false;
			while (std::getline(req_stream, req_line)) {
				if (req_line != req_terminator) {
					// std::cout << "this is not the terminator" << std::endl;
					outfile << req_line + "\n";
				}
				else {
					// std::cout << "THE TERMINATOR" << std::endl;
					terminator_found = true;
					break;
				}
				// std::cout << req_line.length() << ", " << req_terminator.length() << std::endl;
				// std::cout << req_line << std::endl;
			}
			// std::cout << "END OF FILE" << std::endl;
			outfile.close();

			// Write thumbnail
			if (fileIsImage(&out_filename)) {
				Magick::Image thumbnail;
				thumbnail.read(state->doc_root() + out_filename);
				thumbnail.resize("150x150");
				thumbnail.quality(50);
				thumbnail.write(state->doc_root() + "thumbnails/THUMBNAIL_" + out_filename + ".jxl");
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
			http::response<http::empty_body> res;
			res.result(500);
			res.prepare_payload();
			return res;
		}
	}
	else if (req.method() == http::verb::delete_) {
		if (req.target().substr(0, 20) == "/api/delete_message/") {
			http::response<http::string_body> res;
			json request_json = json::parse(req.body());
			if (request_json.contains("key")) {
				std::cout << "Request contains key" << std::endl;
				int slash_index, thread_id, message_id;
				if ((slash_index = req.target().substr(20, req.target().length() - 20).find('/')) != std::string::npos) {
					try {
						thread_id = std::stoi(req.target().substr(20, slash_index));
						message_id = std::stoi(req.target().substr(20 + slash_index + 1, req.target().length() - slash_index - 1 - 20));
					}
					catch (std::invalid_argument const& exception) {
						res.result(400);
						std::cout << "Invalid ID in delete_message URL" << std::endl;
						goto prepare_response_payload;
					}
					catch (std::out_of_range const& exception) {
						res.result(400);
						std::cout << "Out-of-range ID in delete_message URL" << std::endl;
						goto prepare_response_payload;
					}
					if (state->main_board.threadExists(thread_id)) {
						std::string user_key = request_json["key"].template get<std::string>();
						if (message_id == 0) {
							// state->main_board.deleteThread(thread_id, user_key);
							res.result(http::status::unauthorized);
						}
						else {
							if (state->main_board.messageExistsInThread(message_id, thread_id)) {
								if (state->main_board.keyMatchesMessageInThread(user_key, message_id, thread_id)) {
									state->main_board.deleteMessageFromThread(message_id, thread_id);
								}
								else {
									std::cout << "key doesnt match\n";
								}
							}
							else {
								std::cout << "Message " << message_id << " does not exist in thread " << thread_id << std::endl;
							}
						}
						res.result(200);
					}
					else {
						std::cout << "Thread " << thread_id << " does not exist\n";
					}
				}

			}
			else {
				return bad_request("Denied: Request does not contain key\n");
			}
prepare_response_payload:
			res.prepare_payload();
			return res;
		}
		else {
			std::cout << "Unknown target: " << req.target() << std::endl;
			http::response<http::empty_body> res;
			res.result(500);
			res.prepare_payload();
			return res;
		}
	}
	else {
        return bad_request("Unknown HTTP-method");
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
	// This is 25MB
    parser_->body_limit(25 << 20);

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
