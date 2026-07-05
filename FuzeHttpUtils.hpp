#pragma once
#include <boost/program_options/value_semantic.hpp>
#include <sodium.h>
#include <boost/algorithm/string/replace.hpp>
#include <boost/hash2/md5.hpp>
#include <boost/json.hpp>
#include <boost/program_options.hpp>
#include <boost/smart_ptr.hpp>
#include <filesystem>
#include <fstream>
#include <print>
#include <string>

namespace FuzeHttp{
template<typename T>
std::string valueAsString(const T& value);

template<typename T>
requires(requires(const T& val) {std::to_string(val);})
std::string valueAsString(const T& value) {
	return std::to_string(value);
}
template<>
inline std::string valueAsString(const std::string& value) {
	return value;
}

class TemplateMacro {
public:
	constexpr TemplateMacro(std::string token) :token(token) {}
	const std::string token;
	virtual void addOptionToListIfOptional(boost::program_options::options_description& options) = 0;
	virtual std::string string() const = 0;
	virtual bool isOption() const = 0;
	virtual bool includeInFrontend() const { return true; };
};

template<typename OptionType>
requires (std::is_convertible_v<std::remove_pointer_t<OptionType>, std::string> || requires(std::remove_pointer_t<OptionType> o){std::to_string(o);})
class TemplateOption : public TemplateMacro {
public:
	TemplateOption(std::string token, OptionType default_value, std::string description = "") :
		TemplateMacro(token), default_value(default_value), description(description), value(std::make_shared<OptionType>(default_value)) {
	}
	// TemplateOption(std::string token, std::shared_ptr<OptionType>&& value_ptr, OptionType default_value, std::string description = "")
	// 		: TemplateMacro(token), default_value(default_value), description(description), value(value_ptr) {
	// }
	virtual void addOptionToListIfOptional(boost::program_options::options_description& options) override {
		options.add(boost::make_shared<boost::program_options::option_description>( boost::program_options::option_description(this->token.c_str(), boost::program_options::value<OptionType>(value.get())->default_value(default_value), this->description ? description.value().c_str() : "")));
	}
	virtual std::string string() const override {
		return valueAsString(*value);
	}
	virtual bool isOption() const override { return true; };
private:
	std::shared_ptr<OptionType> value;
	OptionType default_value;
	const std::optional<const std::string> description;
};

template<typename OptionType>
requires (std::is_convertible_v<std::remove_pointer_t<OptionType>, std::string> || requires(std::remove_pointer_t<OptionType> o){std::to_string(o);})
class TemplateOptionPtr : public TemplateMacro {
public:
	struct Args {
		std::optional<OptionType> default_value;
		std::optional<const char*> description;
		bool include_in_frontend = true;
	};
	// TemplateOptionPtr(std::string token, OptionType* value_ptr, OptionType default_value, std::string description = "")
	// 		: TemplateMacro(token), default_value(default_value), description(description), value_ptr(value_ptr) {
	// }
	TemplateOptionPtr(std::string token, OptionType* value_ptr, Args args = {})
			: TemplateMacro(token), value_ptr(value_ptr), /*typed_value(value_ptr),*/ default_value(args.default_value), description(args.description), include_in_frontend(args.include_in_frontend) {
		// if (this->default_value)
		// 	this->typed_value.default_value(this->default_value.value());
		// this->value_semantic = std::make_shared<boost::program_options::value_semantic*>(&(this->typed_value));
		// this->value_semantic = &(this->typed_value);
	}
	virtual void addOptionToListIfOptional(boost::program_options::options_description& options) override {
		// boost::program_options::typed_value value(value_ptr);
		// boost::program_options::typed_value value = boost::program_options::value<OptionType>(value_ptr);
		// boost::program_options::value_semantic* value_semantic = &value;
		// options.add(boost::make_shared<boost::program_options::option_description>( boost::program_options::option_description(this->token.c_str(), *value_semantic.get(), this->description ? description.value() : "")));
		if (this->default_value)
			options.add(boost::make_shared<boost::program_options::option_description>( boost::program_options::option_description(this->token.c_str(), boost::program_options::value(value_ptr)->default_value(this->default_value.value()), this->description ? description.value() : "")));
		else
			options.add(boost::make_shared<boost::program_options::option_description>( boost::program_options::option_description(this->token.c_str(), boost::program_options::value(value_ptr), this->description ? description.value() : "")));
	}
	virtual std::string string() const override {
		return valueAsString(*value_ptr);
	}
	virtual bool isOption() const override { return true; };
	virtual bool includeInFrontend() const override { return this->include_in_frontend; };
private:
	OptionType* value_ptr;
	// boost::program_options::typed_value<OptionType> typed_value;
	// boost::program_options::value_semantic* value_semantic;
	// std::shared_ptr<boost::program_options::value_semantic*> value_semantic;
	std::optional<OptionType> default_value;
	const std::optional<const char*> description;
	bool include_in_frontend;
};

template<typename OptionType>
requires (std::is_convertible_v<OptionType, std::string> || requires(OptionType o){std::to_string(o);})
class TemplateConstant : public TemplateMacro {
public:
	TemplateConstant(std::string token, OptionType default_value, std::string description = "") :
		TemplateMacro(token), default_value(default_value), description(description), value(default_value) {
	}
	virtual void addOptionToListIfOptional(boost::program_options::options_description& options) override {}
	virtual std::string string() const override {
		return valueAsString(value);
	}
	virtual bool isOption() const override { return false; };
private:
	OptionType value;
	OptionType default_value;
	const std::optional<const std::string> description;
};

inline bool fileNameEndsWith(const std::string& file_name, const std::string& delimiter) {
	int file_extension_index;
	if ((file_extension_index = file_name.rfind(".")) == -1) {
		file_extension_index = file_name.size();
	}
	return file_name.substr(0, file_extension_index).ends_with(delimiter);
}

inline std::string insertExtensionToFileName(const std::string& file_name, const std::string& extension) {
	int file_extension_index;
	if ((file_extension_index = file_name.rfind(".")) == -1) {
		file_extension_index = file_name.size();
	}
	std::string new_file_name = file_name.substr(0, file_extension_index) + extension + file_name.substr(file_extension_index);
	return new_file_name;
}

// Mysteriously doesnt link when placed in cpp file
inline void applyOptionsToTemplates(const std::vector<TemplateMacro*>& options, const std::filesystem::path& document_root, const std::unordered_map<std::string /*target*/, std::string /*etag*/> manifest_frontend_etags){
	std::println("Adding options to templates...");
	for (auto option : options)
		std::println("{} :: {}", option->token, option->string());
	for (const std::filesystem::directory_entry& dir_entry : std::filesystem::recursive_directory_iterator(document_root)) {
		if (!std::filesystem::is_regular_file(dir_entry))
			continue;
		int file_extension_index;
		if ((file_extension_index = dir_entry.path().filename().string().rfind(".")) == -1) {
			file_extension_index = dir_entry.path().filename().string().size();
		}
		if (!dir_entry.path().filename().string().substr(0, file_extension_index).ends_with(".template"))
			continue;
		std::println("[applyOptionsToTemplates] path: {}", dir_entry.path().string());
		std::ifstream file_template_stream(dir_entry.path());
		std::string out_filename = dir_entry.path().filename().string().substr(0, file_extension_index - sizeof(".template")+1) + ".GENERATED" + dir_entry.path().filename().string().substr(file_extension_index);
		// if (out_filename.starts_with('_'))
		// 	out_filename = out_filename.substr(1);
		std::println(" ->{} ", out_filename);
		std::ofstream file_output_stream(dir_entry.path().parent_path() / out_filename);
		std::string file_line;
		while (std::getline(file_template_stream, file_line)) {
			for (auto option : options) {
				if (option->includeInFrontend())
					boost::replace_all(file_line, std::format("CONFIG_{}", option->token), option->string());
			}
			if (size_t file_token_i; (file_token_i = file_line.find("FILE_")) != std::string::npos) {
				size_t file_token_value_i = file_token_i + sizeof "FILE_";
				// std::println("{}", file_line[file_token_value_i]);
				if (file_line[file_token_value_i - 1] != '"')
					throw std::runtime_error("FILE_ macro requires \" characters around path");
				size_t closing_index = file_line.find('"', file_token_value_i);
				// std::println("{}", file_line[closing_index]);
				if (closing_index == std::string::npos)
					throw std::runtime_error("FILE_ macro missing closing \" character");
				std::string file_token_value = file_line.substr(file_token_value_i, closing_index - file_token_value_i);
				std::println("file_token_value: {}", file_token_value);
				std::filesystem::path file_token_path = file_token_value;
				std::println("file_token_path: {}", file_token_value);
				std::filesystem::path resolved_file_token_path = dir_entry.path().parent_path() / file_token_path;
				std::println("resolved_file_token_path: {}", resolved_file_token_path.string());
				std::filesystem::path proximate_file_token_path = std::filesystem::proximate(resolved_file_token_path, document_root);
				if (auto it = manifest_frontend_etags.find(proximate_file_token_path.string()); it != manifest_frontend_etags.end()) {
					std::println("Found manifest etag! {}", it->second);
					file_line.erase(file_token_i, closing_index+1 - file_token_i);
					file_line.insert(file_token_i, insertExtensionToFileName(file_token_value, it->second));
				}
				else
					throw std::runtime_error(std::format("Etag not found\nPath: {}\n Line: {}", proximate_file_token_path.string(), file_line));


				// std::filesystem::path proximate_file_token_path = std::filesystem::proximate(file_token_path);
				// std::println("resolved_file_token_path: {}", resolved_file_token_path.string());
				// TODO replace file_token_value with cache-busted version by finding path from map
				// std::filesystem::path dependency_path = file_token_value;
			}
			file_output_stream << file_line << std::endl;
		}
		file_output_stream.close();
		file_template_stream.close();
	}
	std::println("Done.");
}

template<typename BoostHashType, typename StringType>
requires (requires(BoostHashType hasher, const StringType& str){hasher.update(str.c_str(), str.length());})
std::string getHash(const StringType& source_data) {
	BoostHashType hash;
	hash.update(source_data.c_str(), source_data.length());
	char hash_base64[sodium_base64_ENCODED_LEN(128 / 8, sodium_base64_VARIANT_URLSAFE_NO_PADDING)];
	sodium_bin2base64(
		hash_base64, sizeof hash_base64,
		hash.result().data(), hash.result().size(),
		// (unsigned char*)key_bytes, 20,
		sodium_base64_VARIANT_URLSAFE_NO_PADDING
	);
	return hash_base64;
}

inline std::string getEtagFromFile(const std::filesystem::path& file) {
	std::string file_last_modified = std::to_string(std::filesystem::last_write_time(file).time_since_epoch().count());
	return getHash<boost::hash2::md5_128>(file_last_modified);
}

std::string writeManifestJson(const std::filesystem::path& manifest_file, const std::unordered_map<std::string /*target*/, std::string /*etag*/>& manifest_frontend_etags, const std::string& combined_hash);
}; // namespace FuzeHttp
