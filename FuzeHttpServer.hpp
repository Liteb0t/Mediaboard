#pragma once
#include "listener.hpp"
#define BOOST_DLL_USE_STD_FS
#include <boost/dll/runtime_symbol_info.hpp>
#include <boost/program_options.hpp>
#include "FuzeHttpUtils.hpp"

namespace FuzeHttp {
class Server {
public:
	Server();
	int processOptions(int argc, char* argv[], std::vector<FuzeHttp::TemplateMacro*> additional_options);
	template<class StateType>
	void run(StateType* state) {
		auto address = boost::asio::ip::make_address("127.0.0.1");
		// The io_context is required for all I/O - see https://www.boost.org/doc/libs/latest/doc/html/boost_asio/overview/basics.html
		boost::asio::io_context io_context;
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

		// if (variable_map.count("create_owner")) {
		// 	std::string invite_key = state->createInvite(static_cast<int>(BUILTIN_GROUPS::OWNER));
		// 	std::cout << std::endl << "Use this link to register the owner account: http://localhost:" << server_port << "/invite/" << invite_key << std::endl;
		// }
		// else if (!state->ownerExists())
		// 	std::println("\nERROR: No owner found. Restart the application with --create_owner");
		// else
		// 	std::cout << "The server can now be accessed from http://localhost:" << server_port << std::endl;
		// std::cout << std::flush;

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
		io_context.run();

		// (If we get here, it means we got a SIGINT or SIGTERM)

		// Block until all the threads exit
		for(auto& t : v)
			t.join();
		if (threads == 1)
			std::cout << "Thread closed." << std::endl;
		else
			std::cout << "All " << threads << " threads closed." << std::endl;
		state->clearExpiredSessions();
		// delete state;
		// delete database_connection;
	}
	FuzeDBI::Connection* db;
	std::filesystem::path document_root;
	std::filesystem::path media_location;
	std::unordered_map<std::string /*target*/, std::string /*etag*/> manifest_frontend_etags;
	std::unordered_map<std::string, std::string> busted_target_to_target;
	std::unordered_set<std::string> files_generated_from_templates;
private:
	// FuzeHttp::State* state;
	const unsigned short threads = 1;
	const unsigned short server_port = 8300;
};
} // namespace FuzeHttp
