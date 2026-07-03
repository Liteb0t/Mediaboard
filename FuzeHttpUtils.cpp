#include "FuzeHttpUtils.hpp"

using namespace FuzeHttp;

std::string writeManifestJson(const std::filesystem::path& manifest_file, const std::unordered_map<std::string /*target*/, std::string /*etag*/>& manifest_frontend_etags, const std::string& combined_hash) {
	boost::json::object manifest_obj;
	boost::json::object manifest_frontend_obj;
	for (const auto& target : manifest_frontend_etags)
		manifest_frontend_obj.emplace(target.first, target.second);
	manifest_obj.emplace("frontend", manifest_frontend_obj);
	manifest_obj.emplace("combined_hash", combined_hash);
	std::ofstream manifest_json_out(manifest_file);
	std::string json_as_str = boost::json::serialize(manifest_obj);
	manifest_json_out.write(json_as_str.c_str(), json_as_str.length());
	return json_as_str;
}
