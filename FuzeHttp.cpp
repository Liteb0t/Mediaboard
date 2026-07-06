// FUZE.page 2026
// The following code is not to be used for AI training. For humans, the MIT license applies.
#include "FuzeHttp.hpp"
#include "PermissionObject.hpp"
#include <boost/beast/http/empty_body.hpp>

char FuzeHttp::fromHex(char ch) {
	return std::isdigit(ch) ? ch - '0' : std::tolower(ch) - 'a' + 10;
}

const std::string forbidden_file_name_chars = "#?+/&";
void FuzeHttp::sanitiseFileName(std::string& file_name) {
	if (file_name[0] == ' ')
		file_name[0] = '_';
	for (int i = 0; i < file_name.length(); i++) {
		if (forbidden_file_name_chars.find(file_name[i]) != -1) {
			file_name[i] = '_';
		}
	}
}

// Return a reasonable mime type based on the extension of a file.
const std::string_view FuzeHttp::getMimeType(const std::string& path) {
	using beast::iequals;
	std::string_view ext = [&path] {
		auto const pos = path.rfind(".");
		if(pos == std::string_view::npos)
			return std::string_view{};
		return std::string_view(path).substr(pos);
	}();
	if(iequals(ext, ".aac"))  return "audio/aac";
	if(iequals(ext, ".flac")) return "audio/flac";
	if(iequals(ext, ".mid"))  return "audio/midi";
	if(iequals(ext, ".midi")) return "audio/midi";
	if(iequals(ext, ".mp3"))  return "audio/mpeg";
	if(iequals(ext, ".oga"))  return "audio/ogg";
	if(iequals(ext, ".ogx"))  return "audio/ogg";
	if(iequals(ext, ".opus")) return "audio/ogg";
	if(iequals(ext, ".js"))   return "application/javascript";
	if(iequals(ext, ".json")) return "application/json";
	if(iequals(ext, ".xml"))  return "application/xml";
	if(iequals(ext, ".swf"))  return "application/x-shockwave-flash";
	if(iequals(ext, ".avif")) return "image/avif";
	if(iequals(ext, ".bmp"))  return "image/bmp";
	if(iequals(ext, ".gif"))  return "image/gif";
	if(iequals(ext, ".heic")) return "image/heic";
	if(iequals(ext, ".heics"))return "image/heic";
	if(iequals(ext, ".ico"))  return "image/vnd.microsoft.icon";
	if(iequals(ext, ".jpe"))  return "image/jpeg";
	if(iequals(ext, ".jpeg")) return "image/jpeg";
	if(iequals(ext, ".jpg"))  return "image/jpeg";
	if(iequals(ext, ".jxl"))  return "image/jxl";
	if(iequals(ext, ".png"))  return "image/png";
	if(iequals(ext, ".tiff")) return "image/tiff";
	if(iequals(ext, ".tif"))  return "image/tiff";
	if(iequals(ext, ".svg"))  return "image/svg+xml";
	if(iequals(ext, ".svgz")) return "image/svg+xml";
	if(iequals(ext, ".webp")) return "image/webp";
	if(iequals(ext, ".htm"))  return "text/html";
	if(iequals(ext, ".html")) return "text/html";
	if(iequals(ext, ".php"))  return "text/html";
	if(iequals(ext, ".css"))  return "text/css";
	if(iequals(ext, ".txt"))  return "text/plain";
	if(iequals(ext, ".flv"))  return "video/x-flv";
	if(iequals(ext, ".mp4"))  return "video/mp4";
	if(iequals(ext, ".webm"))  return "video/webm";
	return "application/text";
}

std::string FuzeHttp::getDecodedURL(boost::string_view raw_URL) {
	// URL decoding in C http://www.geekhideout.com/urlcode.shtml
	std::string decoded_url;
	decoded_url.reserve(raw_URL.length()+1);
	for (boost::string_view::const_iterator i = raw_URL.begin(), n = raw_URL.end(); i != n; i++) {
		std::string::value_type c = (*i);
		if (c == '%') {
			if (i+1 != n && i+2 != n) {
				decoded_url += fromHex(*(i+1)) << 4 | fromHex(*(i+2));
				i += 2;
			}
		}
		else if (c == '+')
			decoded_url += ' ';
		else
			decoded_url +=  c;
	}
	return decoded_url;
}

std::string_view FuzeHttp::getPathName(const std::string& source_URL) {
	// path_name excludes URL parameters (stuff after '?')
	// removes trailing / but leaves first /
	std::string_view path_name = source_URL;
	int decoded_url_questionmark_index = source_URL.rfind('?');
	if (decoded_url_questionmark_index != std::string::npos)
		path_name = path_name.substr(0, decoded_url_questionmark_index);
	std::cout << "path_name: " << path_name << std::endl;
	return path_name;
}

void FuzeHttp::generatePasswordHashHashBase64(char* password_hash_hash_base64, size_t password_hash_hash_base64_len, const char* password_hash_base64, size_t password_hash_base64_len) {
	// hash of password hash in base64 is stored in DB
	unsigned char password_hash_hash[crypto_generichash_BYTES];
	crypto_generichash(
		password_hash_hash, crypto_generichash_BYTES,
		reinterpret_cast<const unsigned char*>(password_hash_base64), password_hash_base64_len,
					   NULL, 0
	);
	sodium_bin2base64(
		password_hash_hash_base64, password_hash_hash_base64_len,
		password_hash_hash, sizeof password_hash_hash,
		sodium_base64_VARIANT_URLSAFE
	);
}

template<>
http::response<http::string_body> FuzeHttp::buildResponse(FuzeHttp::Response basic_response, const http::request<http::string_body, http::basic_fields<std::allocator<char>>>& req) {

	http::response<http::string_body> res{basic_response.status, req.version()};
	res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
	if (basic_response.headers) {
		for (auto& header : basic_response.headers.value())
			res.set(header.first, header.second);
	}
	if (basic_response.error_message)
		res.set("message", basic_response.error_message.value());
	if (basic_response.json) {
		res.set(http::field::content_type, "application/json");
		res.body() = boost::json::serialize(basic_response.json.value());
	}
	else if (basic_response.error_message)
		res.body() = basic_response.error_message.value();
	else if (basic_response.body)
		res.body() = basic_response.body.value();
	res.keep_alive(req.keep_alive());
	res.prepare_payload();
	return res;
}

template<>
http::response<http::file_body> FuzeHttp::buildResponse(FuzeHttp::Response basic_response, const http::request<http::string_body, http::basic_fields<std::allocator<char>>>& req) {
	std::cout << "Attempting to open " << basic_response.file.value() << std::endl;
	// Attempt to open the file
	beast::error_code ec;
	http::file_body::value_type body;
	body.open(basic_response.file.value().c_str(), beast::file_mode::scan, ec);

	if (ec) // Handle an unknown error
		throw std::runtime_error("Unknown error when attempting to open file");

	// Cache the size since we need it after the move
	const uint64_t size = body.size();

	http::response<http::file_body> res{
		std::piecewise_construct,
		std::make_tuple(std::move(body)),
		std::make_tuple(http::status::ok, req.version())
	};
	if (basic_response.headers) {
		for (auto& header : basic_response.headers.value())
			res.set(header.first, header.second);
	}
	res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
	res.set(http::field::content_type, getMimeType(basic_response.file.value().string()));
	res.content_length(size);
	res.keep_alive(req.keep_alive());
	return res;
}

template<>
http::response<http::empty_body> FuzeHttp::buildResponse(FuzeHttp::Response basic_response, const http::request<http::string_body, http::basic_fields<std::allocator<char>>>& req) {

	http::response<http::empty_body> res{basic_response.status, req.version()};
	res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
	if (basic_response.headers) {
		for (auto& header : basic_response.headers.value())
			res.set(header.first, header.second);
	}
	if (basic_response.error_message)
		res.set("message", basic_response.error_message.value());
	res.keep_alive(req.keep_alive());
	res.prepare_payload();
	return res;
}
