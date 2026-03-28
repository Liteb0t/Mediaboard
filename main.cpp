// Fuze Mediaboard was built from an example project by Vinnie Falco.
// https://github.com/vinniefalco/CppCon2018
//------------------------------------------------------------------------------
/*
	WebSocket chat server, multi-threaded

	This implements a multi-user chat room using WebSocket. The
	`io_context` runs on any number of threads, specified at
	the command line.

*/
//------------------------------------------------------------------------------

#include "DatabaseConnectionPostgreSQL.hpp"
#include "DatabaseConnectionSQLite.hpp"
#include "listener.hpp"
#include "migrations.hpp"
#include "shared_state.hpp"
#include <boost/asio/signal_set.hpp>
#include <boost/dll.hpp>
#include <boost/dll/runtime_symbol_info.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/program_options.hpp>
#include <boost/smart_ptr.hpp>
#include <Magick++.h>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

const std::string current_version = "0.1";

int main(int argc, char* argv[]) {
	Magick::InitializeMagick(*argv);  // Required on Windows and MacOS

	// Check command line arguments.
	std::string config_file;
	unsigned short server_port, postgresql_port;
	std::string admin_password, media_location_relative_str, database_engine, sqlite_database_path, postgresql_uri, postgresql_user, postgresql_host, postgresql_database_name;
	int threads;
	bool postgresql_use_uri;
	boost::program_options::options_description command_line_specific_options("Command-line-specific options");
	command_line_specific_options.add_options()
		("create_administrator,a", boost::program_options::value<std::string>(&admin_password), "Create \"Administrator\" account with the specified password.")
		// ("initdb,i", "Initialise the PostgreSQL database")
		("config,c", boost::program_options::value<std::string>(&config_file)->default_value("config.ini"), "location of configuration file.")
		("version,v", "Show version string.")
		("help,h", "Show list of options.");

	// These options can be specified in config.ini
	boost::program_options::options_description universal_options("Universal options");
	universal_options.add_options()
		("media_path,m", boost::program_options::value<std::string>(&media_location_relative_str)->default_value("."),  "File path where user-submitted media is stored.")
		("server_port,p", boost::program_options::value<unsigned short>(&server_port)->default_value(8300), "The port which the server will serve. Make sure it isn't already in use by another service.")
		("database_engine,d", boost::program_options::value<std::string>(&database_engine)->default_value("sqlite"), "Choices are \"postgres\" and \"sqlite\". The latter is recommended for beginners.")
		("postgresql_use_uri", boost::program_options::value<bool>(&postgresql_use_uri)->default_value(false), "If true, use postgresql_uri to connect.")
		("postgresql_uri,u", boost::program_options::value<std::string>(&postgresql_uri)->default_value("fuze_mediaboard@localhost:5432"),  "Connection string for the PostgreSQL database.")
		("postgresql_user,U", boost::program_options::value<std::string>(&postgresql_user)->default_value("mediaboard_server"),  "User which will access the PostgreSQL database.")
		("postgresql_host,h", boost::program_options::value<std::string>(&postgresql_host)->default_value("localhost"),  "Host for the PostgreSQL database.")
		("postgresql_port,p", boost::program_options::value<unsigned short>(&postgresql_port)->default_value(5432), "The port at which the database is available.")
		("postgresql_database_name,n", boost::program_options::value<std::string>(&postgresql_database_name)->default_value("fuze_mediaboard"), "Name of the PostgreSQL database.")
		("sqlite_database_path,s", boost::program_options::value<std::string>(&postgresql_uri)->default_value("database/sqlite_data.db"),  "File where SQLite data is stored.")
		("threads,t", boost::program_options::value<int>(&threads)->default_value(1), "Number of async threads.");

	boost::program_options::options_description command_line_options;
	command_line_options.add(command_line_specific_options).add(universal_options);

	boost::program_options::variables_map variable_map;
	store(boost::program_options::parse_command_line(argc, argv, command_line_options), variable_map);
	boost::program_options::notify(variable_map);

	if (variable_map.count("help")) {
		std::cout << command_line_options << std::endl;
		return 0;
	}
	if (variable_map.count("version")) {
		std::cout << current_version << std::endl;
		return 0;
	}

	// Load config.ini
	std::ifstream config_file_ifstream(config_file.c_str());
	if (config_file_ifstream) {
		std::cout << "Loaded config file" << std::endl;
		store(parse_config_file(config_file_ifstream, universal_options), variable_map);
		notify(variable_map);
	}
	else {
		std::cout << "Could not open config file: " << config_file << ". Default options will be used." << std::endl;
	}

	bool make_migrations;
	// Get the path to this program, so files can be read/written relative to the executable
	std::error_code ec;
	boost::filesystem::path location = boost::dll::program_location(ec).parent_path();
	boost::filesystem::path database_location = location; database_location += "/database";
	if (ec)
		throw("An error occured when attempting to get the current program's location.");
	else
		std::cout << "Server is located at " << location << std::endl;
	if (!boost::filesystem::exists(database_location.string())) {
		std::cout << database_location.string() << " doesn't exist. Creating..." << std::endl;
		boost::filesystem::create_directory(database_location.string());
	}

	DatabaseConnection* database_connection;
	if (database_engine.starts_with("postgres")) {
		if (postgresql_use_uri)
			database_connection = new DatabaseConnectionPostgreSQL(postgresql_uri, current_version);
		else
			database_connection = new DatabaseConnectionPostgreSQL(postgresql_user, postgresql_host, postgresql_port, postgresql_database_name, current_version);
	}
	else if (database_engine.starts_with("sqlite"))
		database_connection = new DatabaseConnectionSQLite(database_location, "sqlite_data.db", current_version);
	else {
		std::cerr << "Error: unknown database engine \"" << database_engine << "\". Must be \"postgres\" or \"sqlite\"." << std::endl;
		return EXIT_FAILURE;
	}

	if (variable_map.count("create_administrator")) {
		db_create_administrator(admin_password.c_str());
		std::cout << "Created 'Administrator' account successfully. Restart the server, click on \"Log-in or Register\", and log in as 'Administrator' using the same password you entered here." << std::endl;
		delete database_connection;
		return 0;
	}
	std::cout << "Set port: " << server_port << std::endl;
	boost::filesystem::path media_location;
	try {
		media_location = boost::filesystem::canonical(media_location_relative_str, location);
	}
	catch (const std::exception* exception) {
		std::cout << exception->what();
	}
	if (!boost::filesystem::exists(media_location.string() + "/media")) {
		std::cout << media_location.string() + "/media" << " doesn't exist. Creating..." << std::endl;
		boost::filesystem::create_directory(media_location.string() + "/media");
	}
	media_location += "/media";
	if (!boost::filesystem::exists(media_location.string() + "/thumbnails")) {
		std::cout << media_location.string() + "/thumbnails" << " doesn't exist. Creating..." << std::endl;
		boost::filesystem::create_directory(media_location.string() + "/thumbnails");
	}
	std::cout << "Set media_location: " << media_location << std::endl;
	std::cout << "Set threads: " << threads << std::endl;
	if (threads > 1)
		std::cout << "Warning: issues may arise from multi-threading" << std::endl;

	auto address = boost::asio::ip::make_address("127.0.0.1");
	// The io_context is required for all I/O - see https://www.boost.org/doc/libs/latest/doc/html/boost_asio/overview/basics.html
	boost::asio::io_context io_context;

	// Create and launch a listening port
	std::cout << "Creating a listening port..." << std::endl;
	boost::shared_ptr<shared_state> state(new shared_state(location, media_location, database_connection));
	state->start();
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

	// Run the I/O service on the requested number of threads
	std::cout << "Running the I/O service..." << std::endl;
	std::vector<std::thread> v;
	v.reserve(threads - 1);
	for(auto i = threads - 1; i > 0; --i) {
		v.emplace_back(
			[&io_context] {
				io_context.run();
			}
		);
	}
	std::cout << "The server can now be accessed from http://localhost:" << server_port << std::endl;
	io_context.run();

	// (If we get here, it means we got a SIGINT or SIGTERM)

	// Block until all the threads exit
	for(auto& t : v)
		t.join();

	if (threads == 1)
		std::cout << "Thread closed." << std::endl;
	else
		std::cout << "All " << threads << " threads closed." << std::endl;
	delete database_connection;

	return EXIT_SUCCESS;
}
