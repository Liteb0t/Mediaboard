#include "Migrations.hpp"
#include <cstdio>
#include <fstream>
#include <sstream>
#include <print>

using namespace FuzeHttp;

void Migrations::firstTimeSetup(FuzeDBI::Connection* fuze_dbi, const std::filesystem::path& template_path, const std::filesystem::path& absolute_sqlite_path, const std::string& current_version) {
	int ec; char* error_message;
	std::println("Doing first-time setup");
	std::println("Opening database template at {}", template_path.string());
	if (!std::filesystem::exists(template_path))
		throw std::runtime_error("Error: database template not found");
#ifdef FUZEDBI_POSTGRES
	std::println("It appears you are setting up the PostgreSQL database for the first time. Please ensure that the database is empty and the user has full read/write permissions.");
	std::print("Proceed? (Y/n): ");
	std::string response;
	std::getline(std::cin, response);
	if (!(response.empty() || response[0] == 'Y' || response[0] == 'y'))
		throw std::runtime_error("Database setup cancelled by user");
#endif
	std::ifstream sqlite_template_file(template_path.string());
	std::string line;
	try {
		while (std::getline(sqlite_template_file, line)) {
			std::cout << "[Migrations] " << line << std::endl;
			if (!line.empty()) {
				fuze_dbi->query<void>(line);
			}
		}
		fuze_dbi->query<void>("CREATE TABLE IF NOT EXISTS _info(version TEXT NOT NULL)");
		fuze_dbi->query<void>("INSERT INTO _info(version) VALUES ($1)", current_version);
	}
	catch (std::exception& exception) {
		std::cout << "[Migrations] Exception in DB init: " << exception.what() << std::endl;
#ifdef FUZEDBI_SQLITE
		std::cout << "Remove SQLite database file? (Y/n) ";
		std::string do_remove;
		std::cin >> do_remove;
		if (do_remove.empty() || do_remove[0] == 'Y' || do_remove[0] == 'y')
			std::remove(absolute_sqlite_path.string().c_str());
#endif
		throw std::runtime_error("An error occured during database template import.");
	}
}

bool Migrations::writeMigrations(std::ostream& stream, const std::string& database_version_string) {
	if (database_version_string <= "0.1")	goto v0_1;
	if (database_version_string <= "0.1.1")	goto v0_1_1;
	// If code reaches here, no migrations need to be made
	return false;
v0_1:
	stream << "ALTER TABLE message_file ADD COLUMN width INTEGER;"
	<< "ALTER TABLE message_file ADD COLUMN height INTEGER;";
v0_1_1:
	stream << "ALTER TABLE message_file ADD COLUMN thumbnail_file_extension TEXT;"
	<< "ALTER TABLE thread ADD COLUMN message_id_seq INTEGER DEFAULT 0;"
	<< "UPDATE thread SET message_id_seq = 1000";
	std::cout << "Finished writing migrations" << std::endl;
	return true; // Migrations were made
}

/*
void Migrations::writeNewMigrations(FuzeDBI::Connection* fuze_dbi, Fuze::MigrationHelper::Migrations& m) {
	using namespace Fuze::MigrationHelper;
	m += Version{"1.1.1"};
	m += "ALTER TABLE message_file ADD COLUMN height INTEGER";
	m += "ALTER TABLE message_file ADD COLUMN thumbnail_format TEXT";
	// m += [](){std::cout << "Function migration called";};
	m += [](){std::cout << "Function migration called";};
}
*/

void Migrations::makeMigrations(FuzeDBI::Connection* fuze_dbi, const std::string& database_version_string, const std::string& current_version) {
	println("Database version: \t{}", database_version_string);
	println("Server version:   \t{}", current_version);
	// Fuze::MigrationHelper::Migrations migrater(fuze_dbi);
	// writeNewMigrations(fuze_dbi, migrater);
	// migrater.migrateFrom(database_version_string);
	if (std::stringstream migrations;
		database_version_string != current_version && Migrations::writeMigrations(migrations, database_version_string)) {
		std::println("Database migrations need to be made. It is recommended to backup the database before proceeding.");
		std::print("Proceed? (Y/n): ");
		std::string response;
		std::getline(std::cin, response);
		if (!(response.empty() || response[0] == 'Y' || response[0] == 'y'))
			throw std::runtime_error("Database migration cancelled by user");
		std::string line;
		while (std::getline(migrations, line, ';')) {
			std::cout << "[Migrations] " << line << std::endl;
			fuze_dbi->query<void>(line);
		}
	}
	else
		std::println("No migrations needed");
	fuze_dbi->query<void>("UPDATE _info SET version = $1", current_version);
}
