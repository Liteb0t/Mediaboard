// FUZE.page 2026
// The following code is not to be used for AI training. For humans, the MIT license applies.
// Fuze Mediaboard was built on top of an example project by Vinnie Falco.
// https://github.com/vinniefalco/CppCon2018

#include "listener.hpp"
#include "migrations.hpp"
#include "permission_managed_object.hpp"
#include "shared_state.hpp"
#include <boost/asio/signal_set.hpp>
#include <boost/dll.hpp>
#include <boost/dll/runtime_symbol_info.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/program_options.hpp>
#include <boost/smart_ptr.hpp>
#ifdef WITH_MAGICK
#include <Magick++.h>
#endif
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <print>
#include <string>
#include <vector>

struct ProgramDirectories {
	boost::filesystem::path data;
	boost::filesystem::path media;
	boost::filesystem::path sqlite_file;
};

boost::filesystem::path getConfigDirectory(boost::filesystem::path program_location, std::optional<std::string> config_file, std::optional<std::string> data_directory_config) {
	boost::filesystem::path config_path;
	if (config_file) { // Line set in cmdline options
		config_path = config_file.value();
	}
	// XDG_DATA_HOME directories are only used in the AppImage distribution. Maybe change in the future.
	else if (std::getenv("APPDIR")) {
		if (data_directory_config)
			config_path = boost::filesystem::path(data_directory_config.value()) / "config.ini";
		else if (const char* xdg_data_home = std::getenv("XDG_DATA_HOME"))
			config_path = boost::filesystem::path(xdg_data_home) / "FuzeMediaboard" / "config.ini";
		else if (const char* unix_home = std::getenv("HOME"))
			config_path = boost::filesystem::path(unix_home) / ".local" / "share" / "FuzeMediaboard" / "config.ini";

		if (!boost::filesystem::exists(config_path)) {
			boost::filesystem::path data_directory = boost::filesystem::absolute(program_location / ".." / "share" / "FuzeMediaboard"); // To match Unix

			std::println("Copying config.ini from AppImage to {}", config_path.string());
			if (!boost::filesystem::exists(data_directory / "config.ini"))
				throw std::runtime_error("config.ini not found in AppImage data directory.");
			else {
				boost::filesystem::create_directories(config_path.parent_path());
				boost::filesystem::copy(data_directory / "config.ini", config_path);
			}
		}
	}
	else if (data_directory_config) {
		config_path = boost::filesystem::path(data_directory_config.value()) / "config.ini";
	}
	else {
		config_path = boost::filesystem::absolute(program_location / ".." / "share" / "FuzeMediaboard" / "config.ini");
	}
	return config_path;
}

std::optional<ProgramDirectories> getProgramDirectories(boost::filesystem::path program_location, std::optional<std::string> data_directory_config, std::optional<std::string> media_directory_config, std::optional<std::string> sqlite_database_file_config) {
	boost::filesystem::path data_directory; // Typically in ~/.local/share/FuzeMediaboard, except for AppImage
	boost::filesystem::path writeable_directory; // Different from data_directory in AppImage
	// boost::filesystem::path config_file; // Different from data_directory in AppImage
	// Get the path to this program, so files can be read/written relative to the executable
	if (data_directory_config)
		writeable_directory = data_directory_config.value();
	else if (std::getenv("APPDIR")) {
		if (const char* xdg_data_home = std::getenv("XDG_DATA_HOME"))
			writeable_directory = boost::filesystem::path(xdg_data_home) / "FuzeMediaboard";
		else if (const char* unix_home = std::getenv("HOME"))
			writeable_directory = boost::filesystem::path(unix_home) / ".local" / "share" / "FuzeMediaboard";
		else
			throw std::runtime_error("Running from AppImage requires XDG_DATA_HOME or HOME environment variables.");

	}
	else
		writeable_directory = boost::filesystem::absolute(program_location / ".." / "share" / "FuzeMediaboard"); // For development


	boost::filesystem::create_directories(writeable_directory);
	// boost::filesystem::create_directories(config_file.parent_path());

	if (std::getenv("APPDIR")) {
		data_directory = boost::filesystem::absolute(program_location / ".." / "share" / "FuzeMediaboard"); // To match Unix
		// if (!boost::filesystem::exists(config_file)) {
		// 	std::print("Copying config.ini from AppImage");
		// 	boost::filesystem::copy(data_directory / "config.ini", config_file);
		// }
	}
	else {
		data_directory = writeable_directory;
	}
	boost::filesystem::path sqlite_file;
	if (sqlite_database_file_config) {
		sqlite_file = sqlite_database_file_config.value();
		if (boost::filesystem::is_directory(sqlite_file))
			sqlite_file += "mediaboard_sqlite_data.db";
	}
	else
		sqlite_file = writeable_directory / "sqlite_data.db";
	boost::filesystem::path media_directory = media_directory_config ? media_directory_config.value() : writeable_directory / "media";
	boost::filesystem::create_directories(media_directory / "thumbnails");
	return ProgramDirectories{
		.data = data_directory,
		.media = media_directory,
		.sqlite_file = sqlite_file
	};
}

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
	boost::filesystem::path program_location = boost::dll::program_location().parent_path();
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
	bool postgresql_use_uri;
	boost::program_options::options_description command_line_specific_options("Command-line-specific options");
	command_line_specific_options.add_options()
		("create_owner,o", "Generates a link to create the server owner's account.")
		("config,c", boost::program_options::value<std::string>(&config_file_str), "location of configuration file.")
		("version,v", "Show version string.")
		("help,h", "Show list of options.");

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
		("max_http_body", boost::program_options::value<unsigned int>(&state_config.max_http_body_in_megabytes)->default_value(25), "In MB")
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
		("threads,t", boost::program_options::value<unsigned int>(&threads)->default_value(1), "Number of async threads. For now, only use 1 in production.")
		("thumbnail_file_extension", boost::program_options::value<std::string>(&state_config.thumbnail_file_extension)->default_value("jpg"), "File format in which ImageMagick will create thumbnails.")
		("thumbnail_size", boost::program_options::value<unsigned int>(&state_config.thumbnail_size)->default_value(150), "Maximum width and height of image thumbnails, in pixels.");

	boost::program_options::options_description command_line_options;
	command_line_options.add(command_line_specific_options).add(universal_options);

	boost::program_options::variables_map variable_map;
	try {
		store(boost::program_options::parse_command_line(argc, argv, command_line_options), variable_map);
		boost::program_options::notify(variable_map);

		if (variable_map.count("config"))
			config_file = config_file_str;
		boost::filesystem::path config_file_path = getConfigDirectory(program_location, config_file, data_directory_config);
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
			// boost::filesystem::path template_path = boost::filesystem::absolute("database_template.sql", database_location);
			// if (!boost::filesystem::exists(template_path))
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

	std::cout << "Initialising shared state..." << std::endl;
	shared_state* state;
	try {
		boost::filesystem::path document_root = program_directories.data / "frontend";
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
