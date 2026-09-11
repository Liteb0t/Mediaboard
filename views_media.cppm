// Copyright (c) 2026, Fuze.page
// Fuze Human-oriented License v1
module;
// #include "views_media.hpp"
#include <boost/beast/http/status.hpp>
// #include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#ifdef WITH_MAGICK
#include <Magick++.h>
#endif
#include "Request.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <print>
export module Mediaboard.Views_media;

import FuzeHttp.Core;
import FuzeHttp.PermissionObject;
import Mediaboard.Message;
import Mediaboard.Permission;
import Mediaboard.State;

using namespace FuzeHttp;

export namespace Mediaboard {

FuzeHttp::Response uploadFile(Mediaboard::State* state, FuzeHttp::Request req) {
	std::optional<Client> client = state->getClientIfExists(req);
	// TODO have UPLOAD_FILE permission modifiable for threads and boards.
	if (!state->clientHasPermission(client, static_cast<int>(PERMISSION::UPLOAD_FILE)))
		return FuzeHttp::Response{.status = http::status::forbidden, .error_message = "You lack permission to upload files."};
	std::istringstream req_stream(req.body());
	std::string req_line;
	std::getline(req_stream, req_line);
	std::string req_terminator = req_line.substr(0, req_line.length()-1) + "--\r";
	// std::cout << "Request ID: " << req_terminator << std::endl;
	std::string out_filename;
	bool empty_line = false;
	while (!empty_line) {
		std::getline(req_stream, req_line, '\n');
		if (req_line == "\r") {
			empty_line = true;
		}
		else if (req_line.substr(0, 19) == "Content-Disposition") {
			int filename_i;
			filename_i = req_line.find("filename", 20) + 10;
			if (filename_i != std::string::npos) {
				int filename_end_i;
				if ((filename_end_i = req_line.find(";", filename_i)) == std::string::npos) {
					filename_end_i = req_line.length() - filename_i - 2;
				}
				std::cout << filename_end_i << std::endl;
				out_filename = req_line.substr(filename_i, filename_end_i);
				std::cout << "out_filename: " << out_filename << std::endl;
			}
		}
		// else if (req_line.substr(0, 13) == "Content-Type") {
		// 	int boundary_i;
		// 	if ((boundary_i = req_line.find("boundary", 13)) != std::string::npos) {
		// 		std::cout << req_line.substr(boundary_i+1, req_line.length()) << std::endl;
		// 	}
		// }
		// std::cout << "line: " << i++ << std::endl << req_line << std::endl;
	}
	if (out_filename.empty()) {
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = "File name not found in POST header"};
	}
	else if (out_filename.length() > static_cast<int>(File::MAX_FILE_NAME)) {
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = std::format("File name length exceeds the server-defined limit of {}", static_cast<int>(File::MAX_FILE_NAME))};
	}
	FuzeHttp::sanitiseFileName(out_filename);
	std::cout << "Sanitised out_filename: " << out_filename << std::endl;

	// Add UUID to filename
	boost::uuids::uuid u = boost::uuids::random_generator()();
	std::string uuid_str = boost::uuids::to_string(u);
	int filename_uuid_index;
	if ((filename_uuid_index = out_filename.rfind(".")) == -1) {
		filename_uuid_index = out_filename.size();
	}
	out_filename.insert(filename_uuid_index, uuid_str);

	const std::filesystem::path out_file_path = state->getMediaLocation() / out_filename;
	// Write to the file
	std::ofstream outfile(out_file_path.string(), std::ios::binary);
	bool is_initial_line = true;
	bool previous_line_ends_with_carriage_return = false;
	while (std::getline(req_stream, req_line)) {
		// std::cout << req_line << std::endl;
		// std::cout << req_line.length() << ", " << req_terminator.length() << std::endl;
		if (req_line != req_terminator) {
			if (previous_line_ends_with_carriage_return) {
				previous_line_ends_with_carriage_return = false;
				outfile << "\r";
			}
			if (!is_initial_line)
				outfile << "\n";
			if (req_line.length() == 0)
				continue;
			else if (req_line.back() == '\r') {
				previous_line_ends_with_carriage_return = true;
				outfile << req_line.substr(0, req_line.length() - 1);
			}
			else
				outfile << req_line;
			is_initial_line = false;
		}
		else {
			break;
		}
	}
	std::cout << "Finished reading data" << std::endl;
	try {
		outfile.exceptions(outfile.failbit);
		outfile.close();
	}
	catch (const std::ios_base::failure& exception) {
		std::stringstream error_message;
		error_message
		<< "Reason: " << exception.what() << '\n'
		<< "Error code: " << exception.code() << "\n";
		std::cerr << "Exception thrown when attempting to save uploaded file.\n" << error_message.str();
		return FuzeHttp::Response{.status = http::status::internal_server_error, .error_message = error_message.str()};
	}
	std::cout << "END OF FILE" << std::endl;
	FuzeHttp::Response response{
		.status = http::status::accepted,
		.headers = {{
			{"File-Name", out_filename}
		}}
	};

	// Create thumbnail if applicable
#ifdef WITH_MAGICK
	const std::string_view file_mime_type = FuzeHttp::getMimeType(out_filename);
	std::print("MIME type: {}", file_mime_type);
	bool create_thumbnail_for_image = state->canCreateThumbnailForImageFormat(file_mime_type);
	bool create_thumbnail_for_video = state->canCreateThumbnailForVideoFormat(file_mime_type);
	std::println(" - create thumbnail? {}", create_thumbnail_for_image || create_thumbnail_for_video);
	unsigned int image_width = 100, image_height = 100;
	bool uploaded_file_has_thumbnail = false;
	if (create_thumbnail_for_image || create_thumbnail_for_video) {
		try {
			Magick::Image thumbnail;
			if (create_thumbnail_for_image) {
				thumbnail.read(out_file_path.string());
				if (file_mime_type != "image/svg+xml") { // stripping image messes up svg files
					Magick::Image image;
					image.read(out_file_path.string());
					if (state->config.strip_metadata) {
						image.autoOrient();
						image.strip(); // Removes metadata
					}
					if (state->config.convert_heic_to_jpg && file_mime_type == "image/heic") {
						image.quality(80);
						image.write(out_file_path.string().substr(0, out_file_path.string().rfind('.'))+".jpg");
						std::println("out_filename was first {}", out_filename);
						out_filename = out_filename.substr(0, out_filename.rfind('.'))+".jpg";
						std::println("out_filename is now {}", out_filename);
					}
					else if (state->config.strip_metadata)
						image.write(out_file_path.string());
				}
			}
			else {
				thumbnail.read(std::format("{}[0]", out_file_path.string())); // read the first frame into ImageMagick ffmpeg delegate
			}
			const Magick::Geometry size = thumbnail.size();
			image_width = size.width();
			image_height = size.height();
			Magick::Geometry thumbnail_dimensions;
			if (size.width() < size.height()) {
				thumbnail_dimensions.width(size.width() < size.height()>>1 ? std::ceil(state->config.thumbnail_size / 2) : std::ceil(state->config.thumbnail_size * (size.width()/size.height())));
				thumbnail_dimensions.height(state->config.thumbnail_size);
			}
			else if (size.height() < size.width()) {
				thumbnail_dimensions.width(state->config.thumbnail_size);
				thumbnail_dimensions.height(size.height() < size.width()>>1 ? std::ceil(state->config.thumbnail_size / 2) : std::ceil(state->config.thumbnail_size * (size.height()/size.width())));
			}
			else {
				thumbnail_dimensions.width(state->config.thumbnail_size);
				thumbnail_dimensions.height(state->config.thumbnail_size);
			}
			thumbnail.resize(thumbnail_dimensions);
			thumbnail.quality(60);
			thumbnail.write(std::format("{}/thumbnails/THUMBNAIL_{}.{}", state->getMediaLocation().string(), out_filename, state->config.thumbnail_file_extension));
			uploaded_file_has_thumbnail = true;
			if (state->config.convert_heic_to_jpg && file_mime_type == "image/heic") {
				std::filesystem::remove(out_file_path);
			}
		}
		catch( Magick::Warning& magick_warning ) {
			std::cerr << "[Magick++] WARNING: " << magick_warning.what() << std::endl << "Image might not be made." << std::endl;
		}
		catch (Magick::Error& magick_error) {
			std::cerr << "[Magick++] ERROR: " << magick_error.what() << std::endl << "Image will therefore not be made." << std::endl;
		}
	}
	if (create_thumbnail_for_image || create_thumbnail_for_video) {
		if (create_thumbnail_for_image || (create_thumbnail_for_video && uploaded_file_has_thumbnail)) {
			response.headers.emplace("Image-Width", std::to_string(image_width));
			response.headers.emplace("Image-Height", std::to_string(image_height));
		}
		if (uploaded_file_has_thumbnail)
			response.headers.emplace("Thumbnail-File-Extension", state->config.thumbnail_file_extension);
	}
#endif
	return response;
}

// TODO find a way to handle multiple directories under one view
FuzeHttp::Response getMedia(Mediaboard::State* state, FuzeHttp::Request req, std::string location) {
	std::cout << "Showing thru getMedia" << std::endl;
	std::string file_name = location;
	int filename_extension_index;
	if ((filename_extension_index = file_name.rfind(".")) == -1) {
		filename_extension_index = file_name.size();
	}
	if (filename_extension_index >= 36) {
		file_name.erase(filename_extension_index - 36, 36);
	}
	std::filesystem::path file_path = state->getMediaLocation() / location;
	if (!std::filesystem::exists(file_path))
		return FuzeHttp::Response{http::status::not_found};
	if (!std::filesystem::is_regular_file(file_path))
		return FuzeHttp::Response{.status = http::status::bad_request, .error_message = "Is a directory"};
	return FuzeHttp::Response{
		.status = http::status::ok,
		.headers = {{
			{"Content-Disposition", std::format("inline; filename=\"{}\"", file_name)}
			// Would conflict with Cache-Control set in FuzeHttp.hpp
			// ,{"Cache-Control", "max-age=31536000, immutable"}
		}},
		.file = file_path.string()
	};
}

FuzeHttp::Response getThumbnail(Mediaboard::State* state, FuzeHttp::Request req, std::string file_path) {
	std::cout << "Showing thru getMedia" << std::endl;
	std::string file_name = file_path;
	int filename_extension_index;
	if ((filename_extension_index = file_name.rfind(".")) == -1) {
		filename_extension_index = file_name.size();
	}
	if (filename_extension_index >= 36) {
		file_name.erase(filename_extension_index - 36, 36);
	}
	return FuzeHttp::Response{
		.status = http::status::ok,
		.file = std::format("{}/thumbnails/{}", state->getMediaLocation().string(), file_path)
	};
}
} // namespace Mediaboard
