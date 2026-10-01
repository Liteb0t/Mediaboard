module;
#include <boost/json.hpp>
#include <filesystem>
export module Mediaboard.Manager.MediaboardServerInformation;

export namespace Mediaboard::Manager {

// Will scan 'servers' directory on startup, and update the database accordingly
struct MediaboardServerInformation {
	std::string site_name;
	std::filesystem::path data_directory; // containing config.ini
	int server_port;
	bool is_valid = true;
	boost::json::array additional_information = {};
	boost::json::object asJson() const {
		return {{
			{"site_name", site_name},
			{"data_directory", data_directory.string()},
			{"server_port", server_port},
			{"is_valid", is_valid},
			{"additional_information", additional_information}
		}};
	}
};
}
