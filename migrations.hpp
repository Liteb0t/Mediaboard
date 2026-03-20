#include <boost/optional.hpp>
#include <fstream>
#include <iostream>
#include <string>

void writeDatabaseVersionFile(std::string parent_directory, std::string database_version) {
	std::ofstream database_version_file(parent_directory + "/database/MEDIABOARD_VERSION");
	database_version_file << database_version;
	database_version_file.close();
}

boost::optional<std::string> getExistingDatabaseVersion(std::string parent_directory) {
	std::ifstream file(parent_directory + "/database/MEDIABOARD_VERSION");
	if (!file.is_open()) {
		return boost::none;
	}
	std::string version_string;
	std::getline(file, version_string);
	if (!version_string.empty())
		return version_string;
	else
		return boost::none;
}

// Returns true if any migrations need to be made by psql
bool writeMigrations(std::string parent_directory, std::string database_version, std::string current_version) {
	std::ofstream file(parent_directory + "/database/migrations.sql");
	// Just imagine this is a switch-case, ok?
	if (database_version == "0.0.5")	goto v0_0_5;
	// if (database_version == "1.0")		goto v1_0;
	// If code reaches here, no migrations need to be made
	file.close();
	return false;
v0_0_5:
	file << "UPDATE permission_collection SET account_id = NULL WHERE account_id = -1;\n";
	file << "UPDATE permission_collection SET permission_group_id = NULL WHERE permission_group_id = -1;\n";
// v1_0:
	std::cout << "Finished writing migrations" << std::endl;
	file.close();
	writeDatabaseVersionFile(parent_directory, current_version);
	return true;
}

/*
void makeMigrations() {

}*/
