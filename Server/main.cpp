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
#include <execinfo.h>
#include <unistd.h>
#include <stdio.h>
#include <execinfo.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

import Mediaboard.MediaboardWebsocketSession;
import FuzeHttp.Server;
import FuzeHttp.ProgramOptions;
import FuzeHttp.Utils;
import Mediaboard.Config;
import Mediaboard.State;
import Mediaboard.URLs;

const std::string current_version = "0.2.2";

using namespace FuzeHttp;
using namespace Mediaboard;

Mediaboard::StateConfig state_config;

// https://stackoverflow.com/a/77336/18658154
void handler(int sig) {
	void *array[10];
	size_t size;

	// get void*'s for all entries on the stack
	size = backtrace(array, 10);

	// print out all the frames to stderr
	fprintf(stderr, "Error: signal %d:\n", sig);
	backtrace_symbols_fd(array, size, STDERR_FILENO);
	exit(1);
}

void baz() {
 int *foo = (int*)-1; // make a bad pointer
  printf("%d\n", *foo);       // causes segfault
}

void bar() { baz(); }
void foo() { bar(); }

int main(int argc, char* argv[]) {
	setvbuf(stdout, nullptr, _IONBF, 0);
	signal(SIGSEGV, handler);   // install our handler
	// foo(); // this will call foo, bar, and baz.  baz segfaults.
#ifdef WITH_MAGICK
	Magick::InitializeMagick(*argv);  // Required on Windows and MacOS
#else
	std::println("Fuze Mediaboard was compiled without ImageMagick support. Certain features such as thumbnail creation will not work.");
#endif
#ifdef WITH_WEBRTC
	rtc::InitLogger(rtc::LogLevel::Info);
#endif
	FuzeHttp::ProgramOptions server_options;
	addProgramOptions(&server_options, &state_config);

	std::println("Initialising server...");

	FuzeHttp::Server<Mediaboard::State, Mediaboard::WebsocketSession> server(current_version);
	std::println("Finished Initialising server...");
	if (int return_code; (return_code = server.processOptions(argc, argv, std::move(server_options), "MediaboardServer")) != -1)
		return return_code;
	std::println("Finished processing options... adding confuig...");
	server.state->config = state_config;
	std::println("Finished adding config... adding URLs...");
	addURLsToController(&server.controller);
	std::println("Running server...");
	server.run();

	return EXIT_SUCCESS;
}
