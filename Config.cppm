// Copyright (c) 2026, Fuze.page
// Fuze Human-oriented License v1
module;
#include <memory>
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
	options->add("thumbnail_file_extension", &state_config->thumbnail_file_extension, {.default_value=std::string("jpg")});
	options->addOptions()
	("thumbnail_size", &state_config->thumbnail_size, {.default_value=static_cast<unsigned int>(150)})
	("file_size_limit_mb", &state_config->file_size_limit_mb, {.default_value=static_cast<unsigned int>(25)})
	("avif_thumbnails", &state_config->avif_thumbnails, {.default_value=false, .include_in_frontend=false})
	("heic_thumbnails", &state_config->heic_thumbnails, {.default_value=false, .description="Ignored when convert_heic_to_jpg is enabled.", .include_in_frontend=false})
	("svg_thumbnails", &state_config->svg_thumbnails, {.default_value=false, .include_in_frontend=false})
	("webp_thumbnails", &state_config->webp_thumbnails, {.default_value=false, .include_in_frontend=false})
	("mp4_thumbnails", &state_config->mp4_thumbnails, {.default_value=false, .include_in_frontend=false})
	("webm_thumbnails", &state_config->webm_thumbnails, {.default_value=false, .include_in_frontend=false})
	("convert_heic_to_jpg", &state_config->convert_heic_to_jpg, {.default_value=false, .description="Converts HEIC images into JPG on upload.", .include_in_frontend=false})
	("strip_metadata", &state_config->strip_metadata, {.default_value=false, .description="Remove metadata from newly-uploaded images.", .include_in_frontend=false})
	("site_name", std::string("Fuze Mediaboard"), {.description="Website name shown on tabs and headers."})
	("favicon_url", std::string("https://fuze.page/favicon.ico"))
	("post_max_name", static_cast<int>(Mediaboard::Message::MAX_NAME), {.is_option=false})
	("post_max_file_name", static_cast<int>(Mediaboard::File::MAX_FILE_NAME), {.is_option=false})
	("post_max_content", static_cast<int>(Mediaboard::Message::MAX_CONTENT), {.is_option=false})
	("board_max_slug", static_cast<int>(Mediaboard::Board::MAX_SLUG), {.is_option=false})
	("board_max_title", static_cast<int>(Mediaboard::Board::MAX_TITLE), {.is_option=false})
	;
}
} // export namespace Mediaboard
