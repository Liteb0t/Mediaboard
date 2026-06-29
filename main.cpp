// FUZE.page 2026
// The following code is not to be used for AI training. For humans, the MIT license applies.
// Fuze Mediaboard was built on top of an example project by Vinnie Falco.
// https://github.com/vinniefalco/CppCon2018

#include "Message.hpp"
#include "listener.hpp"
#include "migrations.hpp"
#include "permission_managed_object.hpp"
#include "shared_state.hpp"
#include <boost/asio/signal_set.hpp>
#include <boost/dll.hpp>
#include <boost/dll/runtime_symbol_info.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/program_options.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/smart_ptr.hpp>
#include <boost/smart_ptr/make_shared_array.hpp>
#include <memory>
#ifdef WITH_MAGICK
#include <Magick++.h>
#endif
#include <filesystem>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <print>
#include <string>
#include <vector>

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

template<typename T>
std::string valueAsString(const T& value);

template<typename T>
requires(requires(const T& val) {std::to_string(val);})
std::string valueAsString(const T& value) {
	return std::to_string(value);
}
template<>
std::string valueAsString(const std::string& value) {
	return value;
}

class TemplateMacro {
public:
	constexpr TemplateMacro(std::string token) :token(token) {}
	const std::string token;
	virtual void addOptionToListIfOptional(boost::program_options::options_description& options) = 0;
	virtual std::string string() const = 0;
	virtual bool isOption() const = 0;
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
	TemplateOptionPtr(std::string token, OptionType* value_ptr, OptionType default_value, std::string description = "")
			: TemplateMacro(token), default_value(default_value), description(description), value_ptr(value_ptr) {
	}
	virtual void addOptionToListIfOptional(boost::program_options::options_description& options) override {
		options.add(boost::make_shared<boost::program_options::option_description>( boost::program_options::option_description(this->token.c_str(), boost::program_options::value<OptionType>(value_ptr)->default_value(default_value), this->description ? description.value().c_str() : "")));
	}
	virtual std::string string() const override {
		return valueAsString(*value_ptr);
	}
	virtual bool isOption() const override { return true; };
private:
	OptionType* value_ptr;
	OptionType default_value;
	const std::optional<const std::string> description;
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

std::vector<TemplateMacro*> template_macros{
	new TemplateOption<std::string>("site_name", "Fuze Mediaboard", "Website name shown on tabs and headers."),
	new TemplateOption<std::string>("favicon_url", "https://fuze.page/favicon.ico"),
	new TemplateOption("show_watermarks", true),
	new TemplateConstant("post_max_name", static_cast<int>(MESSAGE_FIELDS::MAX_NAME)),
	new TemplateConstant("post_max_file_name", static_cast<int>(MESSAGE_FIELDS::MAX_FILE_NAME)),
	new TemplateConstant("post_max_content", static_cast<int>(MESSAGE_FIELDS::MAX_CONTENT)),
	new TemplateConstant("group_max_name", static_cast<int>(Group::MAX_NAME)),
	new TemplateConstant("account_max_username", static_cast<int>(Account::MAX_USERNAME)),
	new TemplateConstant("mediaboard_version", current_version)
};

void applyOptionsToTemplates(const std::vector<TemplateMacro*>& options, const std::filesystem::path& document_root, const std::filesystem::path& template_root) {
	std::println("Adding options to templates...");
	for (auto option : options)
		std::println("{} :: {}", option->token, option->string());
	for (const std::filesystem::directory_entry& dir_entry : std::filesystem::directory_iterator(template_root)) {
		if (std::filesystem::is_regular_file(dir_entry)) {
			std::println("[applyOptionsToTemplates] path: {}", dir_entry.path().string());
			std::ifstream file_template_stream(dir_entry.path());
			std::string out_filename = dir_entry.path().filename().string();
			if (out_filename[0] == '_')
				out_filename = out_filename.substr(1);
			std::print(" ->{} ", out_filename);
			std::ofstream file_output_stream(document_root / out_filename);
			std::string file_contents;
			while (std::getline(file_template_stream, file_contents)) {
				for (auto option : options) {
					boost::replace_all(file_contents, std::format("CONFIG_{}", option->token), option->string());
				}
				file_output_stream << file_contents << std::endl;
			}
			file_output_stream.close();
			file_template_stream.close();
		}
		// if (std::holds_alternative<std::string>(option))
		// 	std::print("")
	}
	std::println("Done.");
}

// struct TemplateOptionsStruct {
// 	std::string site_name;
// 	std::string favicon_url;
// 	int thumbnail_size;
// } template_options_struct;

int main(int argc, char* argv[]) {
#ifdef WITH_MAGICK
	Magick::InitializeMagick(*argv);  // Required on Windows and MacOS
#else
	std::println("Fuze Mediaboard was compiled without ImageMagick support. Certain features such as thumbnail creation will not work.");
#endif
	if (sodium_init() < 0) {
		std::cerr << "libsodium couldn't be initialised" << std::endl;
		return 1;
	}

	std::error_code ec;
	std::filesystem::path program_location = boost::dll::program_location().parent_path().string();
	if (ec)
		throw std::runtime_error("An error occured when attempting to get the current program's location.");
	else
		std::cout << "Server is located at " << program_location << std::endl;

	std::optional<std::string> config_file, data_directory_config, media_directory_config, sqlite_database_file_config;
	// Check command line arguments.
	unsigned short server_port, postgresql_port;
	std::string config_file_str, data_directory_str, media_directory_str, database_engine, sqlite_database_file_str, postgresql_uri, postgresql_user, postgresql_host, thumbnail_file_format, postgresql_database_name;
	unsigned int threads, thumbnail_size;
	StateConfig state_config;
	std::shared_ptr<StateConfig> state_config_shared;
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
		("avif_thumbnails", boost::program_options::value<bool>(&state_config.avif_thumbnails)->default_value(false))
		("heic_thumbnails", boost::program_options::value<bool>(&state_config.heic_thumbnails)->default_value(false), "Ignored when convert_heic_to_jpg is enabled.")
		("svg_thumbnails", boost::program_options::value<bool>(&state_config.svg_thumbnails)->default_value(false))
		("webp_thumbnails", boost::program_options::value<bool>(&state_config.webp_thumbnails)->default_value(false))
		("mp4_thumbnails", boost::program_options::value<bool>(&state_config.mp4_thumbnails)->default_value(false))
		("webm_thumbnails", boost::program_options::value<bool>(&state_config.webm_thumbnails)->default_value(false))
		("convert_heic_to_jpg", boost::program_options::value<bool>(&state_config.convert_heic_to_jpg)->default_value(false), "Converts HEIC images into JPG on upload.")
		("data_directory", boost::program_options::value<std::string>(&data_directory_str))
		// ("file_size_limit_mb", boost::program_options::value<unsigned int>(&state_config.file_size_limit_mb)->default_value(25), "In MB")
		("media_directory,m", boost::program_options::value<std::string>(&media_directory_str),  "File path where user-submitted media is stored. data_directory is used if none is specified.")
		("sqlite_database_file,s", boost::program_options::value<std::string>(&sqlite_database_file_str),  "File where SQLite data is stored. data_directory is used if none is specified.")
		("server_port,p", boost::program_options::value<unsigned short>(&server_port)->default_value(8300), "The port which the server will serve. Make sure it isn't already in use by another service.")
		("strip_metadata", boost::program_options::value<bool>(&state_config.strip_metadata)->default_value(false), "Remove metadata from newly-uploaded images.")
		("postgresql_use_uri", boost::program_options::value<bool>(&postgresql_use_uri)->default_value(false), "If true, use postgresql_uri to connect.")
		("postgresql_uri,u", boost::program_options::value<std::string>(&postgresql_uri)->default_value("fuze_mediaboard@localhost:5432"),  "Connection string for the PostgreSQL database.")
		("postgresql_user,U", boost::program_options::value<std::string>(&postgresql_user)->default_value("mediaboard_server"),  "User which will access the PostgreSQL database.")
		("postgresql_host,h", boost::program_options::value<std::string>(&postgresql_host)->default_value("localhost"),  "Host for the PostgreSQL database.")
		("postgresql_port,p", boost::program_options::value<unsigned short>(&postgresql_port)->default_value(5432), "The port at which the database is available.")
		("postgresql_database_name,n", boost::program_options::value<std::string>(&postgresql_database_name)->default_value("fuze_mediaboard"), "Name of the PostgreSQL database.")
		("threads,t", boost::program_options::value<unsigned int>(&threads)->default_value(1), "Number of async threads. For now, only use 1 in production.");
		// ("thumbnail_file_extension", boost::program_options::value<std::string>(&state_config.thumbnail_file_extension)->default_value("jpg"), "File format in which ImageMagick will create thumbnails.");
		// ("thumbnail_size", boost::program_options::value<unsigned int>(&state_config.thumbnail_size)->default_value(150), "Maximum width and height of image thumbnails, in pixels.");

	// Macros which link to state_config
	template_macros.push_back(new TemplateOptionPtr("thumbnail_file_extension", &state_config.thumbnail_file_extension, std::string("jpg")));
	template_macros.push_back(new TemplateOptionPtr("thumbnail_size", &state_config.thumbnail_size, static_cast<unsigned int>(150)));
	template_macros.push_back(new TemplateOptionPtr("file_size_limit_mb", &state_config.file_size_limit_mb, static_cast<unsigned int>(25)));

	for (auto macro : template_macros) {
		macro->addOptionToListIfOptional(universal_options);
	}

	boost::program_options::options_description command_line_options;
	command_line_options.add(command_line_specific_options).add(universal_options);

	boost::program_options::variables_map variable_map;
	try {
		store(boost::program_options::parse_command_line(argc, argv, command_line_options), variable_map);
		boost::program_options::notify(variable_map);

		if (variable_map.count("config"))
			config_file = config_file_str;
		std::filesystem::path config_file_path = getConfigDirectory(program_location, config_file, data_directory_config);
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
		return 0;
	}
	if (variable_map.count("version")) {
		std::cout << current_version << std::endl;
		return 0;
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
		return 1;
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
	FuzeDBI::Connection* fuze_database_interface;
	try {
#ifdef FUZEDBI_POSTGRES
		fuze_database_interface = new FuzeDBI::Connection(postgresql_user, postgresql_host, postgresql_port, postgresql_database_name);
#elifdef FUZEDBI_SQLITE
		print("sqlite_database_file: {}", program_directories.sqlite_file.string());

		fuze_database_interface = new FuzeDBI::Connection(program_directories.sqlite_file.string());
#endif
		std::optional<std::string> version_string;
		bool version_string_found = false;
		try {
			version_string = fuze_database_interface->query<std::optional<std::string>>("SELECT version FROM _info");
			version_string_found = true;
		}
		catch(const std::exception& exception) {
			std::cout << "Version string not found in database" << std::endl;
		}
		if (version_string_found && version_string) {
			// std::cout << "Version " <<	version_string.value() << std::endl;
			Migrations::makeMigrations(fuze_database_interface, version_string.value(), state_config);
		}
		else {
			// std::filesystem::path template_path = std::filesystem::absolute("database_template.sql", database_location);
			// if (!std::filesystem::exists(template_path))
			// 	throw std::runtime_error(std::format("Database template file {} not found.", template_path.string()));
			Migrations::firstTimeSetup(fuze_database_interface, program_directories.data / "database_template.sql", program_directories.sqlite_file.string());
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

	auto address = boost::asio::ip::make_address("127.0.0.1");
	// The io_context is required for all I/O - see https://www.boost.org/doc/libs/latest/doc/html/boost_asio/overview/basics.html
	boost::asio::io_context io_context;

	std::filesystem::path document_root = program_directories.data / "frontend";
	std::filesystem::path template_root = program_directories.data / "frontend" / "templates";
	try {
		applyOptionsToTemplates(template_macros, document_root, template_root);
	}
	catch (const std::exception& exception) {
		std::println(std::cerr, "An error occured when generating frontend files: {}", exception.what());
		return 1;
	}

	std::cout << "Initialising shared state..." << std::endl;
	shared_state* state;
	try {
		state = new shared_state(document_root, program_directories.media, state_config, fuze_database_interface);
		state->start();
	}
	catch (const std::exception& exception) {
		std::cerr << "[shared_state] " << exception.what() << std::endl;
		return 1;
	}
	// Create and launch a listening port
	std::cout << "Creating a listening port..." << std::endl;
	boost::make_shared<listener>(
		io_context,
		boost::asio::ip::tcp::endpoint{address, server_port},
		state
	)->run();

	// Capture SIGINT and SIGTERM to perform a clean shutdown
	std::cout << "Setting signals..." << std::endl;
	boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
	signals.async_wait(
		[&io_context](boost::system::error_code const&, int) {
			// Stop the io_context. This will cause run()
			// to return immediately, eventually destroying the
			// io_context and any remaining handlers in it.
			io_context.stop();
		}
	);

	if (variable_map.count("create_owner")) {
		std::string invite_key = state->createInvite(static_cast<int>(BUILTIN_GROUPS::OWNER));
		std::cout << std::endl << "Use this link to register the owner account: http://localhost:" << server_port << "/invite/" << invite_key << std::endl;
	}
	else if (!state->ownerExists())
		std::println("\nERROR: No owner found. Restart the application with --create_owner");
	else
		std::cout << "The server can now be accessed from http://localhost:" << server_port << std::endl;
	std::cout << std::flush;

	// Run the I/O service on the requested number of threads
	// std::cout << "Running the I/O service..." << std::endl;
	std::vector<std::thread> v;
	v.reserve(threads - 1);
	for(auto i = threads - 1; i > 0; --i) {
		v.emplace_back(
			[&io_context] {
				io_context.run();
			}
		);
	}
	io_context.run();

	// (If we get here, it means we got a SIGINT or SIGTERM)

	// Block until all the threads exit
	for(auto& t : v)
		t.join();
	// if (threads == 1)
	// 	std::cout << "Thread closed." << std::endl;
	// else
	// 	std::cout << "All " << threads << " threads closed." << std::endl;
	state->clearExpiredSessions();
	// delete state;
	// delete database_connection;

	return EXIT_SUCCESS;
}
