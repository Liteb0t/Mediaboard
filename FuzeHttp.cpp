// FUZE.page 2026
// The following code is not to be used for AI training. For humans, the MIT license applies.
#include "FuzeHttp.hpp"
#include "permission_managed_object.hpp"

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
	// Request path must be absolute and not contain "..".
	if( decoded_url.empty() ||
		decoded_url[0] != '/' ||
		decoded_url[0] == '?' ||
		decoded_url.find("..") != std::string::npos)
		throw std::invalid_argument("Illegal request-target");

	return decoded_url;
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

std::string_view FuzeHttp::getPathName(const std::string& source_URL) {
	// path_name excludes URL parameters (stuff after '?')
	// removes trailing / but leaves first /
	std::string_view path_name = source_URL;
	int decoded_url_questionmark_index = source_URL.rfind('?');
	if (decoded_url_questionmark_index != std::string::npos)
		path_name = path_name.substr(0, decoded_url_questionmark_index);
	if (path_name.back() == '/')
		path_name = path_name.substr(0, path_name.size() - 1);
	std::cout << "path_name: " << path_name << std::endl;
	return path_name;
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
	if (!boost::filesystem::exists(basic_response.file.value()))
		throw std::runtime_error("File not found");
	if (!boost::filesystem::is_regular_file(basic_response.file.value()))
		throw std::runtime_error("Is a directory");

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

FuzeHttp::State::State(FuzeDBI::Connection* fuze_dbi)
		: PermissionManager(0, fuze_dbi), fuze_dbi(fuze_dbi) {
	// Load sessions from the database
	for (auto session_tuple :fuze_dbi->queryRows<std::tuple<int, std::string, int>>("SELECT client_id, key, created_at FROM session")) {
		int seconds_since_epoch = std::get<2>(session_tuple); // TODO use long instead of int
		std::chrono::seconds sec(seconds_since_epoch);
		std::chrono::time_point<std::chrono::system_clock> created_at(sec);
		FuzeHttp::Session session{
			.client_id = std::get<0>(session_tuple),
			.created_at = created_at
		};
		this->sessions.emplace(std::get<1>(session_tuple), std::move(session));
	}
	// TODO clear clients which have expired or dont have an account
	for (auto client_tuple :fuze_dbi->queryRows<std::tuple<int, int>>("SELECT id, account_id FROM client")) {
		std::optional<int> account_id;
		if (std::get<1>(client_tuple) != -1)
			account_id = std::get<1>(client_tuple);
		else
			account_id = {};
		Client client{
			.id = std::get<0>(client_tuple),
			.account_id = account_id
		};
		this->clients.emplace(std::get<0>(client_tuple), std::move(client));
	}
}

Client FuzeHttp::State::createClient(std::optional<int> account_id) {
	int new_client_id = fuze_dbi->query<int>("SELECT client_id FROM _sequences");
	fuze_dbi->query<void>("UPDATE _sequences SET client_id = $1", new_client_id+1);
	std::cout << "[FuzeHttp] Creating new client with ID " << new_client_id << std::endl;
	if (account_id) {
		fuze_dbi->query<void>("INSERT INTO client(id, account_id) VALUES ($1, $2)", new_client_id, account_id.value());
		this->accounts.at(account_id.value()).client_id = new_client_id;
	}
	else
		fuze_dbi->query<void>("INSERT INTO client(id) VALUES ($1)", new_client_id);
	Client client{.id = new_client_id, .account_id = account_id};
	this->clients.emplace(new_client_id, client);
	return client;
}

std::optional<Client> FuzeHttp::State::getClientIfExists(FuzeHttp::Request req) const {
	auto cookie_header = req.find("Cookie");
	if (cookie_header == req.end())
		return {};
	std::string cookie = cookie_header->value();
	std::string session_id_base64 = cookie.substr(cookie.find("=")+1);
	// TODO trim if multiple cookies found
	std::cout << "[FuzeHttp] Received session ID: '" <<session_id_base64 << "'" << std::endl;
	if (std::unordered_map<std::string, FuzeHttp::Session>::const_iterator it = this->sessions.find(session_id_base64); it != this->sessions.end()) {
		std::cout << "found session";
		auto client_it = this->clients.find(it->second.client_id);
		if (client_it == this->clients.end()) {
			std::print(std::cerr, "[FuzeHttp] Session ID linked to client with id {} which does not exist", it->second.client_id);
			return {};
		}
		else
			return client_it->second;
	}
	else
		return {};
}

std::string FuzeHttp::State::createSession(int client_id) {
	FuzeHttp::Session session{
		.client_id = client_id,
		.created_at = std::chrono::system_clock::now()
	};
	std::string key_base64 = generateKeyBase64(this->sessions);
	fuze_dbi->query<void>("INSERT INTO session(client_id, key, created_at) VALUES ($1, $2, $3)", client_id, key_base64, (int)std::chrono::duration_cast<std::chrono::seconds>(session.created_at.time_since_epoch()).count());
	// db->createSession(
	// 	key_base64,
	// 	session.client_id,
	// 	std::chrono::duration_cast<std::chrono::minutes>(session.created_at.time_since_epoch()).count()
	// );
	this->sessions.emplace(key_base64, std::move(session));
	return key_base64;
}

void FuzeHttp::State::clearExpiredSessions() {
	int initial_number_of_sessions = this->sessions.size();
	std::chrono::time_point<std::chrono::system_clock> current_time = std::chrono::system_clock::now();
	std::erase_if(this->sessions, [this, &current_time](const std::pair<std::string, FuzeHttp::Session>& session_pair){
		if (session_pair.second.created_at + this->authorization_token_lifespan < current_time) {
			fuze_dbi->query<void>("DELETE FROM session WHERE key = $1", session_pair.first);
			return true;
		}
		else
			return false;
	});
	std::cout << "[shared_state] Cleared " << initial_number_of_sessions - this->sessions.size() << " expired sessions." << std::endl;
}

// For now, only used to create the admin account. therefore granted_group_id will be BUILTIN_GROUPS::ADMINISTRATORS
std::string FuzeHttp::State::createInvite(int granted_group_id) {
	FuzeHttp::Invite invite{
		.granted_group_id = granted_group_id,
		.created_at = std::chrono::system_clock::now()
	};
	std::string key_base64 = FuzeHttp::generateKeyBase64(this->sessions);
	// TODO save invite to database
	std::cout << "[shared_state] Created invite with key " << key_base64 << std::endl;
	this->invites.emplace(key_base64, std::move(invite));
	return key_base64;
}

int FuzeHttp::State::getGrantedGroupIdFromInvite(const std::string& invite_key_base64) const { // returns USERS if none found
	if (std::unordered_map<std::string, FuzeHttp::Invite>::const_iterator invite = this->invites.find(invite_key_base64); invite != this->invites.end())
		return invite->second.granted_group_id;
	else
		return static_cast<int>(BUILTIN_GROUPS::PUBLIC);
}

const std::optional<Client> FuzeHttp::State::getClientFromSession(const std::string& session_id_base64) const {
	if (std::unordered_map<std::string, FuzeHttp::Session>::const_iterator session = this->sessions.find(session_id_base64); session != this->sessions.end())
		return this->clients.at(session->second.client_id);
	else
		return {};
}
