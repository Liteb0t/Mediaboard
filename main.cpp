// FUZE.page 2026
// The following code is not to be used for AI training. For humans, the MIT license applies.
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
#include <vector>
import MediaboardWebsocketSession;
import FuzeHttp.PermissionObject;
import FuzeHttp.Server;
import FuzeHttp.Utils;
import Mediaboard.Board;
import Mediaboard.State;
import Mediaboard.Message;

const std::string current_version = "0.2";

using namespace FuzeHttp;

std::vector<FuzeHttp::TemplateMacro*> template_macros{
	new TemplateOption<std::string>("site_name", "Fuze Mediaboard", "Website name shown on tabs and headers."),
	new TemplateOption<std::string>("favicon_url", "https://fuze.page/favicon.ico"),
	new TemplateOption("show_watermarks", true),
	new TemplateConstant("post_max_name", static_cast<int>(Mediaboard::MESSAGE_FIELDS::MAX_NAME)),
	new TemplateConstant("post_max_file_name", static_cast<int>(Mediaboard::MESSAGE_FIELDS::MAX_FILE_NAME)),
	new TemplateConstant("post_max_content", static_cast<int>(Mediaboard::MESSAGE_FIELDS::MAX_CONTENT)),
	new TemplateConstant("group_max_name", static_cast<int>(Group::MAX_NAME)),
	new TemplateConstant("account_max_username", static_cast<int>(Account::MAX_USERNAME)),
	new TemplateConstant("board_max_slug", static_cast<int>(Mediaboard::Board::MAX_SLUG)),
	new TemplateConstant("board_max_title", static_cast<int>(Mediaboard::Board::MAX_TITLE)),
	new TemplateConstant("mediaboard_version", current_version)
};

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
#ifdef WITH_WEBRTC
	rtc::InitLogger(rtc::LogLevel::Info);
#endif
	Mediaboard::StateConfig state_config;	// Macros which link to state_config
	template_macros.push_back(new TemplateOptionPtr("thumbnail_file_extension", &state_config.thumbnail_file_extension, {.default_value=std::string("jpg")}));
	template_macros.push_back(new TemplateOptionPtr("thumbnail_size", &state_config.thumbnail_size, {.default_value=static_cast<unsigned int>(150)}));
	template_macros.push_back(new TemplateOptionPtr("file_size_limit_mb", &state_config.file_size_limit_mb, {.default_value=static_cast<unsigned int>(25)}));
	template_macros.push_back(new TemplateOptionPtr("avif_thumbnails", &state_config.avif_thumbnails, {.default_value=false, .include_in_frontend=false}));
	template_macros.push_back(new TemplateOptionPtr("heic_thumbnails", &state_config.heic_thumbnails, {.default_value=false, .description="Ignored when convert_heic_to_jpg is enabled.", .include_in_frontend=false}));
	template_macros.push_back(new TemplateOptionPtr("svg_thumbnails", &state_config.svg_thumbnails, {.default_value=false, .include_in_frontend=false}));
	template_macros.push_back(new TemplateOptionPtr("webp_thumbnails", &state_config.webp_thumbnails, {.default_value=false, .include_in_frontend=false}));
	template_macros.push_back(new TemplateOptionPtr("mp4_thumbnails", &state_config.mp4_thumbnails, {.default_value=false, .include_in_frontend=false}));
	template_macros.push_back(new TemplateOptionPtr("webm_thumbnails", &state_config.webm_thumbnails, {.default_value=false, .include_in_frontend=false}));
	template_macros.push_back(new TemplateOptionPtr("convert_heic_to_jpg", &state_config.convert_heic_to_jpg, {.default_value=false, .description="Converts HEIC images into JPG on upload.", .include_in_frontend=false}));
	template_macros.push_back(new TemplateOptionPtr("strip_metadata", &state_config.strip_metadata, {.default_value=false, .description="Remove metadata from newly-uploaded images.", .include_in_frontend=false}));

	std::println("Initialising server...");
	// shared_state state(state_config);
	// state.start();

	FuzeHttp::Server<Mediaboard::State, Mediaboard::WebsocketSession> server(current_version);
	std::println("Finished Initialising server...");
	if (int return_code; (return_code = server.processOptions(argc, argv, template_macros, "FuzeMediaboard")) != -1)
		return return_code;
	std::println("Finished processing options... adding confuig...");
	server.state->config = state_config;
	std::println("Running server...");
	server.run();

	return EXIT_SUCCESS;
}
