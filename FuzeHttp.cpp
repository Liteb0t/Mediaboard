#include "FuzeHttp.hpp"

char FuzeHttp::fromHex(char ch) {
	return std::isdigit(ch) ? ch - '0' : std::tolower(ch) - 'a' + 10;
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
