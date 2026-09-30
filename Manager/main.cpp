// Copyright (c) 2026, Fuze.page
// Fuze Human-oriented License v1
// Fuze Mediaboard was built on top of an example project by Vinnie Falco.
// https://github.com/vinniefalco/CppCon2018

// #include "WebsocketSession.hpp"
#ifdef WITH_MAGICK
#include <Magick++.h>
#endif
#ifdef WITH_WEBRTC
#include <rtc/global.hpp>
#endif
#include <print>
#include <string>

#include <signal.h>
#include <csignal>
import FuzeHttp.Server;
import FuzeHttp.ProgramOptions;
import FuzeHttp.Utils;
import Mediaboard.Manager.Config;
import Mediaboard.Manager.State;
import Mediaboard.Manager.URLs;

const std::string current_version = "0.2.3";

using namespace FuzeHttp;
using namespace Mediaboard::Manager;

int main(int argc, char* argv[]) {
	FuzeHttp::ProgramOptions server_options;
	addProgramOptions(&server_options);

	std::println("Initialising MediaboardManager...");
	FuzeHttp::Server<State> server(current_version);
	std::println("Finished Initialising MediaboardManager...");
	if (int return_code; (return_code = server.processOptions(argc, argv, std::move(server_options), "MediaboardManager")) != -1)
		return return_code;
	std::println("Finished processing options... adding confuig...");
	// server.state->config = state_config;
	std::println("Finished adding config... adding URLs...");
	addURLsToController(&server.controller);
	std::println("Running MediaboardManager...");

	return server.run();;
}
