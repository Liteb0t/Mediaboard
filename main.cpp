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
#include "shared_state.hpp"
// #include "db_interface.h"
#include <Magick++.h>
#include <boost/asio/signal_set.hpp>
#include <boost/smart_ptr.hpp>
#include <boost/program_options.hpp>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

const std::string version_string = "0.0.5";

int
main(int argc, char* argv[])
{
	Magick::InitializeMagick(*argv);  // Required on Windows and MacOS

    // Check command line arguments.
	std::string config_file;
	unsigned short port;
	std::string admin_password, doc_root, database_name;
	int threads;
	boost::program_options::options_description command_line_specific_options("Command-line-specific options");
	command_line_specific_options.add_options()
		("create_administrator,a", boost::program_options::value<std::string>(&admin_password), "Create \"Administrator\" account with the specified password.")
		("make_migrations", "Add columns to database for faster migration to 0.0.5.")
		("config,c", boost::program_options::value<std::string>(&config_file)->default_value("config.ini"), "location of configuration file.")
		("version,v", "Show version string.")
		("help,h", "Show list of options.");

	boost::program_options::options_description universal_options("Universal options");
	universal_options.add_options()
		("database,d", boost::program_options::value<std::string>(&database_name),  "Name of the postgresql database.")
		("media_path,m", boost::program_options::value<std::string>(&doc_root),  "File path where user-submitted media is stored.")
		("port,p", boost::program_options::value<unsigned short>(&port), "The port which the server will serve.")
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
		std::cout << version_string << std::endl;
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
	if (!variable_map.count("database")) {
		database_name = "fuze_mediaboard";
		std::cout << "\"database\" not found in config. Using default " << database_name << std::endl;
	}
	else
		std::cout << "Set the database to " << database_name << std::endl;

	// Establish database connection
	db_connect(database_name.c_str());

	if (variable_map.count("create_administrator")) {
		db_create_administrator(admin_password.c_str());
		std::cout << "Created 'Administrator' account successfully" << std::endl;
		return 0;
	}
	// TODO remove after 0.0.5 release
	if (variable_map.count("make_migrations")) {
		db_make_migrations();
		std::cout << "permission_object_id added to threads. Do not run this command again." << std::endl;
		return 0;
	}
	if (!variable_map.count("media_path")) {
		doc_root = ".";
	}
	if (!variable_map.count("port")) {
		port = 8300;
	}
	std::cout << "Set port: " << port << std::endl;
	std::cout << "Set doc_root:" << doc_root << std::endl;
	std::cout << "Set threads: " << threads << std::endl;

    auto address = net::ip::make_address("127.0.0.1");
    // The io_context is required for all I/O
    net::io_context ioc;

    // Create and launch a listening port
	std::cout << "Creating a listening port..." << std::endl;
	boost::shared_ptr<shared_state> state(new shared_state(doc_root));
	state->start();
    boost::make_shared<listener>(
        ioc,
        tcp::endpoint{address, port},
        state)->run();

    // Capture SIGINT and SIGTERM to perform a clean shutdown
	std::cout << "Setting signals..." << std::endl;
    net::signal_set signals(ioc, SIGINT, SIGTERM);
    signals.async_wait(
        [&ioc](boost::system::error_code const&, int)
        {
            // Stop the io_context. This will cause run()
            // to return immediately, eventually destroying the
            // io_context and any remaining handlers in it.
            ioc.stop();
        });

    // Run the I/O service on the requested number of threads
	std::cout << "Running the I/O service..." << std::endl;
    std::vector<std::thread> v;
    v.reserve(threads - 1);
    for(auto i = threads - 1; i > 0; --i)
        v.emplace_back(
        [&ioc]
        {
            ioc.run();
        });
    ioc.run();

    // (If we get here, it means we got a SIGINT or SIGTERM)

    // Block until all the threads exit
    for(auto& t : v)
        t.join();
	db_disconnect();

    return EXIT_SUCCESS;
}
