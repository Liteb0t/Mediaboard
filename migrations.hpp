#include <boost/optional.hpp>
#include <fstream>
#include <iostream>
#include <string>

// void writeMigrations(const std::string version_string) {}

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

void writeMigrations(std::string parent_directory, std::string database_version, std::string current_version) {
	std::ofstream file(parent_directory + "/database/migrations.sql");
	// Just imagine this is a switch-case, ok?
	if (database_version == "0.0.5")	goto v0_0_5;
	// if (database_version == "1.0")		goto v1_0;
	goto skip_migrations; // No migrations needed from this database version
v0_0_5:
	file << "UPDATE TEST do stuff;" << std::endl;
	file << "UPDATE TEST do MORE stuff!!!;" << std::endl;
// v1_0:
	std::cout << "Finished writing migrations" << std::endl;
skip_migrations:
	file.close();

	writeDatabaseVersionFile(parent_directory, current_version);
}

/*
void makeMigrations() {

}*/
