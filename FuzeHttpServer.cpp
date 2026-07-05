#include "FuzeHttpServer.hpp"
#include "FuzeHttp.hpp"
#include "migrations.hpp"
#include <fstream>
#include <iostream>

struct ProgramDirectories {
	std::filesystem::path data;
	std::filesystem::path media;
	std::filesystem::path sqlite_file;
};

std::filesystem::path getConfigDirectory(std::filesystem::path program_location, std::optional<std::string> config_file, std::optional<std::string> data_directory_config) {
	std::filesystem::path config_path;
	if (config_file) { // Line set in cmdline options
		config_path = config_file.value();
	}
	// XDG_DATA_HOME directories are only used in the AppImage distribution. Maybe change in the future.
	else if (std::getenv("APPDIR")) {
		if (data_directory_config)
			config_path = std::filesystem::path(data_directory_config.value()) / "config.ini";
		else if (const char* xdg_data_home = std::getenv("XDG_DATA_HOME"))
			config_path = std::filesystem::path(xdg_data_home) / "FuzeMediaboard" / "config.ini";
		else if (const char* unix_home = std::getenv("HOME"))
			config_path = std::filesystem::path(unix_home) / ".local" / "share" / "FuzeMediaboard" / "config.ini";

		if (!std::filesystem::exists(config_path)) {
			std::filesystem::path data_directory = std::filesystem::absolute(program_location / ".." / "share" / "FuzeMediaboard"); // To match Unix

			std::println("Copying config.ini from AppImage to {}", config_path.string());
			if (!std::filesystem::exists(data_directory / "config.ini"))
				throw std::runtime_error("config.ini not found in AppImage data directory.");
			else {
				std::filesystem::create_directories(config_path.parent_path());
				std::filesystem::copy(data_directory / "config.ini", config_path);
			}
		}
	}
	else if (data_directory_config) {
		config_path = std::filesystem::path(data_directory_config.value()) / "config.ini";
	}
	else {
		config_path = std::filesystem::absolute(program_location / ".." / "share" / "FuzeMediaboard" / "config.ini");
	}
	return config_path;
}

std::optional<ProgramDirectories> getProgramDirectories(std::filesystem::path program_location, std::optional<std::string> data_directory_config, std::optional<std::string> media_directory_config, std::optional<std::string> sqlite_database_file_config) {
	std::filesystem::path data_directory; // Typically in ~/.local/share/FuzeMediaboard, except for AppImage
	std::filesystem::path writeable_directory; // Different from data_directory in AppImage
	// std::filesystem::path config_file; // Different from data_directory in AppImage
	// Get the path to this program, so files can be read/written relative to the executable
	if (data_directory_config)
		writeable_directory = data_directory_config.value();
	else if (std::getenv("APPDIR")) {
		if (const char* xdg_data_home = std::getenv("XDG_DATA_HOME"))
			writeable_directory = std::filesystem::path(xdg_data_home) / "FuzeMediaboard";
		else if (const char* unix_home = std::getenv("HOME"))
			writeable_directory = std::filesystem::path(unix_home) / ".local" / "share" / "FuzeMediaboard";
		else
			throw std::runtime_error("Running from AppImage requires XDG_DATA_HOME or HOME environment variables.");
	}
	else
		writeable_directory = std::filesystem::absolute(program_location / ".." / "share" / "FuzeMediaboard"); // For development


	std::filesystem::create_directories(writeable_directory);
	// std::filesystem::create_directories(config_file.parent_path());

	if (std::getenv("APPDIR")) {
		data_directory = std::filesystem::absolute(program_location / ".." / "share" / "FuzeMediaboard"); // To match Unix
		// if (!std::filesystem::exists(config_file)) {
		// 	std::print("Copying config.ini from AppImage");
		// 	std::filesystem::copy(data_directory / "config.ini", config_file);
		// }
	}
	else {
		data_directory = writeable_directory;
	}
	std::filesystem::path sqlite_file;
	if (sqlite_database_file_config) {
		sqlite_file = sqlite_database_file_config.value();
		if (std::filesystem::is_directory(sqlite_file))
			sqlite_file += "mediaboard_sqlite_data.db";
	}
	else
		sqlite_file = writeable_directory / "sqlite_data.db";
	std::filesystem::path media_directory;
	if (media_directory_config)
		media_directory = media_directory_config.value();
	else
		media_directory = writeable_directory / "media";
	std::filesystem::create_directories(media_directory / "thumbnails");
	return ProgramDirectories{
		.data = data_directory,
		.media = media_directory,
		.sqlite_file = sqlite_file
	};
}

FuzeHttp::Server::Server() {
	if (sodium_init() < 0) {
		std::cerr << "libsodium couldn't be initialised" << std::endl;
		return;
	}
}

int FuzeHttp::Server::processOptions(int argc, char* argv[], std::vector<FuzeHttp::TemplateMacro*> additional_options) {
	std::error_code ec;
	std::filesystem::path program_location = boost::dll::program_location().parent_path();
	if (ec)
		throw std::runtime_error("An error occured when attempting to get the current program's location.");
	else
		std::cout << "Server is located at " << program_location << std::endl;

	std::optional<std::string> config_file, data_directory_config, media_directory_config, sqlite_database_file_config;
	// Check command line arguments.
	unsigned short server_port, postgresql_port;
	std::string config_file_str, data_directory_str, media_directory_str, database_engine, sqlite_database_file_str, postgresql_uri, postgresql_user, postgresql_host, thumbnail_file_format, postgresql_database_name;
	unsigned int threads, thumbnail_size;
	bool postgresql_use_uri;
	boost::program_options::options_description command_line_specific_options("Command-line-specific options");
	command_line_specific_options.add_options()
		("create_owner,o", "Generates a link to create the server owner's account.")
		("config,c", boost::program_options::value<std::string>(&config_file_str), "location of configuration file.")
		("version,v", "Show version string.")
		("help,h", "Show list of options.");

	// std::string site_name, favicon_url;
	// boost::shared_ptr<boost::program_options::option_description> desc( new boost::program_options::option_description("site_name", boost::program_options::value<std::string>(&site_name)));

	// TemplateOption favicon_url_opt("favicon_url", &favicon_url);

	// These options can be specified in config.ini
	boost::program_options::options_description universal_options("Universal options");
	universal_options.add_options()
		("data_directory", boost::program_options::value<std::string>(&data_directory_str))
		// ("file_size_limit_mb", boost::program_options::value<unsigned int>(&state_config.file_size_limit_mb)->default_value(25), "In MB")
		("media_directory,m", boost::program_options::value<std::string>(&media_directory_str),  "File path where user-submitted media is stored. data_directory is used if none is specified.")
		("sqlite_database_file,s", boost::program_options::value<std::string>(&sqlite_database_file_str),  "File where SQLite data is stored. data_directory is used if none is specified.")
		("server_port,p", boost::program_options::value<unsigned short>(&server_port)->default_value(8300), "The port which the server will serve. Make sure it isn't already in use by another service.")
		("postgresql_use_uri", boost::program_options::value<bool>(&postgresql_use_uri)->default_value(false), "If true, use postgresql_uri to connect.")
		("postgresql_uri,u", boost::program_options::value<std::string>(&postgresql_uri)->default_value("fuze_mediaboard@localhost:5432"),  "Connection string for the PostgreSQL database.")
		("postgresql_user,U", boost::program_options::value<std::string>(&postgresql_user)->default_value("mediaboard_server"),  "User which will access the PostgreSQL database.")
		("postgresql_host,h", boost::program_options::value<std::string>(&postgresql_host)->default_value("localhost"),  "Host for the PostgreSQL database.")
		("postgresql_port,p", boost::program_options::value<unsigned short>(&postgresql_port)->default_value(5432), "The port at which the database is available.")
		("postgresql_database_name,n", boost::program_options::value<std::string>(&postgresql_database_name)->default_value("fuze_mediaboard"), "Name of the PostgreSQL database.")
		("threads,t", boost::program_options::value<unsigned int>(&threads)->default_value(1), "Number of async threads. For now, only use 1 in production.");
		// ("thumbnail_file_extension", boost::program_options::value<std::string>(&state_config.thumbnail_file_extension)->default_value("jpg"), "File format in which ImageMagick will create thumbnails.");
		// ("thumbnail_size", boost::program_options::value<unsigned int>(&state_config.thumbnail_size)->default_value(150), "Maximum width and height of image thumbnails, in pixels.");

	for (auto option : additional_options) {
		option->addOptionToListIfOptional(universal_options);
	}

	boost::program_options::options_description command_line_options;
	command_line_options.add(command_line_specific_options).add(universal_options);
	std::filesystem::path config_file_path;

	boost::program_options::variables_map variable_map;
	try {
		store(boost::program_options::parse_command_line(argc, argv, command_line_options), variable_map);
		boost::program_options::notify(variable_map);

		if (variable_map.count("config"))
			config_file = config_file_str;
		config_file_path = getConfigDirectory(program_location, config_file, data_directory_config);
		// Load config.ini
		std::ifstream config_file_ifstream(config_file_path.string());
		if (config_file_ifstream) {
			std::cout << "Loaded config file " << config_file_path << std::endl;
			store(parse_config_file(config_file_ifstream, universal_options), variable_map);
			boost::program_options::notify(variable_map);
		}
		else {
			std::cout << "Could not find config.ini file. Default options will be used." << std::endl;
		}
	}
	catch (const std::exception& exception) {
		std::cout << exception.what() << std::endl;
		return 1;
	}

	if (variable_map.count("help")) {
		std::cout << command_line_options << std::endl;
		return 2;
	}
	if (variable_map.count("version")) {
		std::cout << current_version << std::endl;
		return 2;
	}
	if (variable_map.count("data_directory"))
		data_directory_config = data_directory_str;
	if (variable_map.count("media_directory")) {
		std::println("media_directory config option found");
		media_directory_config = media_directory_str;
	}
	else
		std::println("media_directory config option not found");
	if (variable_map.count("sqlite_database_file"))
		sqlite_database_file_config = sqlite_database_file_str;
	ProgramDirectories program_directories;
	std::optional<ProgramDirectories> program_directories_opt = getProgramDirectories(program_location, data_directory_config, media_directory_config, sqlite_database_file_config);
	if (!program_directories_opt) {
		std::cerr << "Mediaboard setup was cancelled by the user." << std::endl;
		return 3;
	}
	else
		program_directories = program_directories_opt.value();
	std::cout << "Data:\t" << program_directories.data << std::endl
#ifdef FUZEDBI_SQLITE
		<< "SQLite:\t" << program_directories.sqlite_file << std::endl
#endif
		<< "Media:\t" << program_directories.media << std::endl;

	std::println("FuzeDBI interface: {}", FUZEDBI_DB);
	bool make_migrations;
	// FuzeDBI::Connection* fuze_database_interface;
	try {
#ifdef FUZEDBI_POSTGRES
		this->db = new FuzeDBI::Connection(postgresql_user, postgresql_host, postgresql_port, postgresql_database_name);
#elifdef FUZEDBI_SQLITE
		print("sqlite_database_file: {}", program_directories.sqlite_file.string());

		this->db = new FuzeDBI::Connection(program_directories.sqlite_file.string());
#endif
		std::optional<std::string> version_string;
		bool version_string_found = false;
		try {
			version_string = this->db->query<std::optional<std::string>>("SELECT version FROM _info");
			version_string_found = true;
		}
		catch(const std::exception& exception) {
			std::cout << "Version string not found in database" << std::endl;
		}
		if (version_string_found && version_string) {
			// std::cout << "Version " <<	version_string.value() << std::endl;
			Migrations::makeMigrations(this->db, version_string.value());
		}
		else {
			// std::filesystem::path template_path = std::filesystem::absolute("database_template.sql", database_location);
			// if (!std::filesystem::exists(template_path))
			// 	throw std::runtime_error(std::format("Database template file {} not found.", template_path.string()));
			Migrations::firstTimeSetup(this->db, program_directories.data / "database_template.sql", program_directories.sqlite_file.string());
		}
		std::cout << "Set port: " << server_port << std::endl;
	}
	catch (const std::exception& exception) {
		std::cerr << exception.what() << std::endl;
		return 1;
	}
	std::cout << "Set threads: " << threads << std::endl;
	if (threads > 1)
		std::cout << "Warning: issues may arise from multi-threading" << std::endl;

	this->media_location = program_directories.media;
	this->document_root = program_directories.data / "frontend";
	std::filesystem::path manifest_file = program_directories.data / "manifest.json";
	// std::filesystem::path template_root = program_directories.data / "frontend" / "templates";
	// boost::bimap<std::string, std::string> path_to_busted_path;
	// std::unordered_map<std::string, std::filesystem::path> busted_target_to_path;

	// std::unordered_map<std::string , std::string > manifest_frontend_etags;
	// std::unordered_set<std::string> files_generated_from_templates;
	try {
		std::optional<std::string> old_combined_hash;
		bool manifest_file_existed;
		if (std::filesystem::exists(manifest_file)) {
			manifest_file_existed = true;
			std::ifstream manifest_json_in(manifest_file);
			std::string file_line, json_as_str;
			while (std::getline(manifest_json_in, file_line))
				json_as_str += file_line;
			boost::json::object manifest_obj = boost::json::parse(json_as_str).as_object();
			for (const auto& frontend_json_entry : manifest_obj.at("frontend").as_object()) {
				std::filesystem::path frontend_file_path = std::string(frontend_json_entry.key());
				if (!std::filesystem::is_regular_file(frontend_file_path))
					continue;
				if (fileNameEndsWith(frontend_file_path.filename(), ".template"))
					continue;
				if (fileNameEndsWith(frontend_file_path.filename(), ".GENERATED"))
					continue;
				// std::filesystem::path frontend_file_path = std::filesystem::proximate(frontend_file.path(), document_root);
				if (!manifest_frontend_etags.contains(frontend_file_path.string()))
					manifest_frontend_etags.emplace(std::string(frontend_json_entry.key()), frontend_json_entry.value().as_string());
			}
			old_combined_hash = manifest_obj.at("combined_hash").as_string();
		}
		else
			manifest_file_existed = false;
		// create manifest JSON OBJECT
		boost::json::object manifest_obj;
		boost::json::object manifest_frontend_json_obj;
		for (const auto& frontend_file : std::filesystem::recursive_directory_iterator(document_root)) {
			if (!std::filesystem::is_regular_file(frontend_file))
				continue;
			std::filesystem::path frontend_file_path = std::filesystem::proximate(frontend_file.path(), document_root);
			// TODO fix bug where two starts are required to add to files_generated_from_templates
			if (fileNameEndsWith(frontend_file_path.filename(), ".GENERATED")) {
				this->files_generated_from_templates.emplace(
					frontend_file_path.string().substr(0, frontend_file_path.string().rfind(".GENERATED")) +
					frontend_file_path.string().substr(frontend_file_path.string().rfind('.')));
			}
			else if (!this->manifest_frontend_etags.contains(frontend_file_path.string())) {
				std::string new_etag = getEtagFromFile(frontend_file);
				if (!fileNameEndsWith(frontend_file.path().filename(), ".template"))
					this->manifest_frontend_etags.emplace(frontend_file_path.string(), new_etag);
				manifest_frontend_json_obj.emplace(frontend_file_path.string(), new_etag);
			}
		}
		std::string config_hash = getHash<boost::hash2::md5_128>(std::to_string(std::filesystem::last_write_time(config_file_path).time_since_epoch().count()));
		std::string new_frontend_hash = getHash<boost::hash2::md5_128>(boost::json::serialize(manifest_frontend_json_obj));
		std::string new_combined_hash = getHash<boost::hash2::md5_128>(config_hash + new_frontend_hash);
		manifest_obj.emplace("frontend", manifest_frontend_json_obj);
		manifest_obj.emplace("combined_hash", new_combined_hash);
		std::string new_json_as_str = boost::json::serialize(manifest_obj);
		std::ofstream manifest_json_out(manifest_file);
		manifest_json_out.write(new_json_as_str.c_str(), new_json_as_str.length());
		// if combined_hash is different, write to file and process templates

		// writeManifestJson(manifest_file, manifest_frontend_etags, config_file_path);

		// json_as_str = writeManifestJson(manifest_file, manifest_frontend_etags);

		std::println("manifest_frontend_etags:");
		for (const auto& target : manifest_frontend_etags)
			std::println("{} :: {}", target.first, target.second);

		if (!old_combined_hash || (old_combined_hash.value() != new_combined_hash))
			FuzeHttp::applyOptionsToTemplates(additional_options, document_root, manifest_frontend_etags);
		else
			std::println("No changes to frontend detected.");
	}
	catch (const std::exception& exception) {
		std::println(std::cerr, "An error occured when generating frontend files: {}", exception.what());
		return 1;
	}
	// TODO pass to FuzeHttp::State
	// std::unordered_map<std::string, std::string> busted_target_to_target;
	for (const auto& target : manifest_frontend_etags)
		this->busted_target_to_target.emplace(FuzeHttp::insertExtensionToFileName(target.first, target.second), target.first);
	std::println("Busted target to target:");
	for (const auto& target : busted_target_to_target)
		std::println("{} :: {}", target.first, target.second);
	for (const std::string& target : files_generated_from_templates)
		std::println("Target to file generated from template: {}", target);
	return -1;
}
