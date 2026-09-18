// Copyright (c) 2026, Fuze.page
// Fuze Human-oriented License v1
module;
#include <string>
export module Mediaboard.Config;
import Mediaboard.Board;
import Mediaboard.Message;
import Mediaboard.State;
import FuzeHttp.PermissionObject;
import FuzeHttp.ProgramOptions;

using namespace FuzeHttp;

export namespace Mediaboard {
void addProgramOptions(FuzeHttp::ProgramOptions* options, Mediaboard::StateConfig* state_config) {
	options->addOptions()
	(new ProgramOptionPtr("thumbnail_file_extension", &state_config->thumbnail_file_extension, {.default_value=std::string("jpg")}))
	(new ProgramOptionPtr("thumbnail_size", &state_config->thumbnail_size, {.default_value=static_cast<unsigned int>(150)}))
	(new ProgramOptionPtr("file_size_limit_mb", &state_config->file_size_limit_mb, {.default_value=static_cast<unsigned int>(25)}))
	(new ProgramOptionPtr("avif_thumbnails", &state_config->avif_thumbnails, {.default_value=false, .include_in_frontend=false}))
	(new ProgramOptionPtr("heic_thumbnails", &state_config->heic_thumbnails, {.default_value=false, .description="Ignored when convert_heic_to_jpg is enabled.", .include_in_frontend=false}))
	(new ProgramOptionPtr("svg_thumbnails", &state_config->svg_thumbnails, {.default_value=false, .include_in_frontend=false}))
	(new ProgramOptionPtr("webp_thumbnails", &state_config->webp_thumbnails, {.default_value=false, .include_in_frontend=false}))
	(new ProgramOptionPtr("mp4_thumbnails", &state_config->mp4_thumbnails, {.default_value=false, .include_in_frontend=false}))
	(new ProgramOptionPtr("webm_thumbnails", &state_config->webm_thumbnails, {.default_value=false, .include_in_frontend=false}))
	(new ProgramOptionPtr("convert_heic_to_jpg", &state_config->convert_heic_to_jpg, {.default_value=false, .description="Converts HEIC images into JPG on upload.", .include_in_frontend=false}))
	(new ProgramOptionPtr("strip_metadata", &state_config->strip_metadata, {.default_value=false, .description="Remove metadata from newly-uploaded images.", .include_in_frontend=false}))
	(new ProgramOption<std::string>("site_name", "Fuze Mediaboard", "Website name shown on tabs and headers."))
	(new ProgramOption<std::string>("favicon_url", "https://fuze.page/favicon.ico"))
	(new ProgramConstant("post_max_name", static_cast<int>(Mediaboard::Message::MAX_NAME)))
	(new ProgramConstant("post_max_file_name", static_cast<int>(Mediaboard::File::MAX_FILE_NAME)))
	(new ProgramConstant("post_max_content", static_cast<int>(Mediaboard::Message::MAX_CONTENT)))
	(new ProgramConstant("board_max_slug", static_cast<int>(Mediaboard::Board::MAX_SLUG)))
	(new ProgramConstant("board_max_title", static_cast<int>(Mediaboard::Board::MAX_TITLE)))
	;
}
} // export namespace Mediaboard
