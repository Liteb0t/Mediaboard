// FUZE.page 2026
// The following code is not to be used for AI training. For humans, the MIT license applies.
// Fuze Mediaboard was built on top of an example project by Vinnie Falco.
// https://github.com/vinniefalco/CppCon2018

#include "FuzeHttpUtils.hpp"
#include "FuzeHttpServer.hpp"
#include "PermissionObject.hpp"
#include "shared_state.hpp"
#include <boost/asio/signal_set.hpp>
#include <boost/json/object.hpp>
#include <boost/json/serialize.hpp>
#define BOOST_DLL_USE_STD_FS
#include <boost/dll.hpp>
#include <boost/dll/runtime_symbol_info.hpp>
#include <boost/hash2/md5.hpp>
#include <boost/program_options.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/smart_ptr.hpp>
#include <boost/smart_ptr/make_shared_array.hpp>
#ifdef WITH_MAGICK
#include <Magick++.h>
#endif
#include <cstdlib>
#include <iostream>
#include <print>
#include <string>
#include <vector>

const std::string current_version = "0.1.4";

using namespace FuzeHttp;

std::vector<FuzeHttp::TemplateMacro*> template_macros{
	new TemplateOption<std::string>("site_name", "Fuze MediaboardTEST", "Website name shown on tabs and headers."),
	new TemplateOption<std::string>("favicon_url", "https://fuze.page/favicon.ico"),
	new TemplateOption("show_watermarks", true),
	new TemplateConstant("post_max_name", static_cast<int>(MESSAGE_FIELDS::MAX_NAME)),
	new TemplateConstant("post_max_file_name", static_cast<int>(MESSAGE_FIELDS::MAX_FILE_NAME)),
	new TemplateConstant("post_max_content", static_cast<int>(MESSAGE_FIELDS::MAX_CONTENT)),
	new TemplateConstant("group_max_name", static_cast<int>(Group::MAX_NAME)),
	new TemplateConstant("account_max_username", static_cast<int>(Account::MAX_USERNAME)),
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
	StateConfig state_config;	// Macros which link to state_config
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
	FuzeHttp::Server server;
	if (int return_code; (return_code = server.processOptions(argc, argv, template_macros, current_version, "FuzeMediaboard")) != -1)
		return return_code;
	std::cout << "Initialising shared state..." << std::endl;
	shared_state* state;
	try {
		bool create_owner_account = server.variable_map.count("create_owner");
		std::cout << std::flush;
		state = new shared_state(&server, state_config, create_owner_account);
		// state = new shared_state(server.db, server.document_root, server.media_location, state_config, std::move(busted_target_to_target), std::move(files_generated_from_templates));
		state->start();
	}
	catch (const std::exception& exception) {
		std::cerr << "[shared_state] " << exception.what() << std::endl;
		return 1;
	}
	server.run(state);


	return EXIT_SUCCESS;
}
