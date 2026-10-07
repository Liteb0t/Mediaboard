// Copyright (c) 2026, Fuze.page
// Fuze Human-oriented License v1
module;
#include <expected>
#include <filesystem>
#include <iostream>
#include <memory>
#include <print>
#include <string>
export module Mediaboard.Installer;
import HomeDirLibrary;

export namespace Mediaboard {
struct InstallLocation {
    InstallLocation(std::string program_folder_name, std::filesystem::path program_root /*will be usr/../, so FuzeMediaboard */, std::string version) : program_root(program_root) {
    	std::println("program_root: {}", program_root.string());
    	this->parent_location = std::filesystem::path(HomeDirLibrary::getDataDir()) / program_folder_name;
		this->install_version_location = parent_location / "versions" / version;
    }
    std::filesystem::path program_root;
    std::filesystem::path parent_location;
	std::filesystem::path install_version_location;
};

struct ValidatedInstallLocation {
    ValidatedInstallLocation(InstallLocation install_location) : install_location(install_location) {}
    void doInstall() {
		std::filesystem::create_directories(install_location.install_version_location);
		std::println("Will copy {} to {}", install_location.program_root.string(), install_location.install_version_location.string());
		std::filesystem::copy(install_location.program_root, install_location.install_version_location, std::filesystem::copy_options::recursive);

		std::filesystem::create_directories(install_location.parent_location / "servers");
	}
	InstallLocation install_location;
};

std::expected<ValidatedInstallLocation, std::string> validateInstallLocation(InstallLocation install_location) {
	if (std::filesystem::exists(install_location.install_version_location)) {
		const std::string error_message = std::format("Version is already installed at {}.", install_location.install_version_location.string());
		std::println(std::cerr, "Error: {}", error_message);
		return std::unexpected(error_message);
	}
	return ValidatedInstallLocation(install_location);
};

} // export namespace Mediaboard
