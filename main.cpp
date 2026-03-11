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
#include <format>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

const std::string current_version = "0.1";

int main(int argc, char* argv[]) {
	Magick::InitializeMagick(*argv);  // Required on Windows and MacOS

	// Check command line arguments.
	std::string config_file;
	unsigned short port, database_port;
	std::string admin_password, media_location_relative_str, database_name, database_host;
	int threads;
	bool manage_cluster;
	boost::program_options::options_description command_line_specific_options("Command-line-specific options");
	command_line_specific_options.add_options()
		("create_administrator,a", boost::program_options::value<std::string>(&admin_password), "Create \"Administrator\" account with the specified password.")
		// ("initdb,i", "Initialise the Postgres database")
		("config,c", boost::program_options::value<std::string>(&config_file)->default_value("config.ini"), "location of configuration file.")
		("version,v", "Show version string.")
		("help,h", "Show list of options.");

	// These options can be specified in config.ini
	boost::program_options::options_description universal_options("Universal options");
	universal_options.add_options()
		("database,d", boost::program_options::value<std::string>(&database_name)->default_value("fuze_mediaboard"),  "Name of the postgresql database.")
		("database_host,h", boost::program_options::value<std::string>(&database_host)->default_value("localhost"),  "Address of where the DB is hosted.")
		("database_port,P", boost::program_options::value<unsigned short>(&database_port)->default_value(5400),  "The port which the database serves.")
		("manage_cluster,c", boost::program_options::value<bool>(&manage_cluster)->default_value(true), "Whether the database will be managed by Fuze Mediaboard.")
		("media_path,m", boost::program_options::value<std::string>(&media_location_relative_str)->default_value("."),  "File path where user-submitted media is stored.")
		("port,p", boost::program_options::value<unsigned short>(&port)->default_value(8300), "The port which the server will serve. Make sure it isn't in use by another service.")
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
		std::cout << "Could not open config file: " << config_file << std::endl;
	}

	bool make_migrations, first_time_setup;
	// Get the path to this program, so files can be read/written relative to the executable
	std::error_code ec;
	boost::filesystem::path location = boost::dll::program_location(ec);
	if (ec)
		throw("An error occured when attempting to get the current program's location.");
	std::string parent_directory = location.parent_path().string();
	if (!boost::filesystem::exists(parent_directory + "/database")) {
		std::cout << parent_directory + "/database" << " doesn't exist. Creating..." << std::endl;
		boost::filesystem::create_directory(parent_directory + "/database");
	}
	 // First time setup
	if (!boost::filesystem::exists(parent_directory + "/database/MEDIABOARD_VERSION")) {
		first_time_setup = true;
		make_migrations = false;
		writeDatabaseVersionFile(parent_directory, current_version);
		if (manage_cluster) {
			std::cout << "Performing first-time database setup..." << std::endl;
			std::system(std::format("initdb -D {}/database/cluster", parent_directory).c_str());
		}
	}
	else { // Not first time setup - the software may be out of sync with the database
		first_time_setup = false;
		boost::optional<std::string> database_version_string = getExistingDatabaseVersion(parent_directory);
		if (database_version_string) {
			std::cout << "Found database version: " << database_version_string.value() << std::endl;
			if (database_version_string != current_version) {
				writeMigrations(parent_directory, database_version_string.value(), current_version);
				make_migrations = true;
			}
			else {
				std::cout << "Database is up-to-date" << std::endl;
				make_migrations = false;
			}
		}
		else {
			std::cerr << "Could not find database/MEDIABOARD_VERSION file. Creating a new one whilst assuming the DB is up-to-date..." << std::endl;
			writeDatabaseVersionFile(parent_directory, current_version);
			make_migrations = false;
		}
	}
	if (manage_cluster) {
		// TODO remove after DB interface is rewritten in C++
		std::system(std::format("pg_ctl -D {}/database/cluster stop", parent_directory, database_port).c_str());

		std::cout << "Starting database..." << std::endl;
		std::cout << std::format("pg_ctl -D {}/database/cluster -o \"-p {}\" -l {}/database/log.txt start", parent_directory, database_port, parent_directory) << std::endl;
		int ret = std::system(std::format("pg_ctl -D {}/database/cluster -o \"-p {}\" -l {}/database/log.txt start", parent_directory, database_port, parent_directory).c_str());
		if (ret) // The database could not be started, so terminate the program
			return ret;
	}
	if (first_time_setup) {
		std::system(std::format("createuser --host={} --port={} mediaboard_server", database_host, database_port).c_str());
		std::system(std::format("createdb --host={} --port={} fuze_mediaboard", database_host, database_port).c_str());
		std::system(std::format("psql --host={} --port={} {} -f {}/database_template.sql", database_host, database_port, database_name, parent_directory).c_str());
		std::system(std::format("psql --host={} --port={} {} -f {}/default_groups.sql", database_host, database_port, database_name, parent_directory).c_str());
	}
	if (make_migrations) {
		std::system(std::format("psql --host={} --port={} {} -f {}/database/migrations.sql", database_host, database_port, database_name, parent_directory).c_str());
	}

	// Establish database connection
	db_connect(database_name.c_str(), database_port);

	if (variable_map.count("create_administrator")) {
		db_create_administrator(admin_password.c_str());
		std::cout << "Created 'Administrator' account successfully. Restart the server, click on \"Log-in or Register\", and log in as 'Administrator' using the same password you entered here." << std::endl;
		db_disconnect();

		if (manage_cluster) {
			std::cout << "Stopping database..." << std::endl;
			std::system(std::format("pg_ctl -D {}/database/cluster stop", parent_directory, database_port).c_str());
		}
		return 0;
	}
	std::cout << "Set port: " << port << std::endl;
	boost::filesystem::path media_location_relative(media_location_relative_str);
	boost::filesystem::path media_location;
	try {
		media_location = boost::filesystem::canonical(media_location_relative, location.parent_path());
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
	boost::shared_ptr<shared_state> state(new shared_state(location.parent_path(), media_location));
	state->start();
	boost::make_shared<listener>(
		io_context,
		boost::asio::ip::tcp::endpoint{address, port},
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
	std::cout << "The server can now be accessed from http://localhost:" << port << std::endl;
	io_context.run();

	// (If we get here, it means we got a SIGINT or SIGTERM)

	// Block until all the threads exit
	for(auto& t : v)
		t.join();
	db_disconnect();

	if (manage_cluster) {
		std::cout << "Stopping database..." << std::endl;
		std::system(std::format("pg_ctl -D {}/database/cluster stop", parent_directory, database_port).c_str());
	}

	return EXIT_SUCCESS;
}
