#pragma once
#include "FuzeDBI.hpp"
// #include "FuzeMigrationHelper.hpp"
#include <iostream>
#include <filesystem>
#include <string>

namespace FuzeHttp {
namespace Migrations {
// Populates database with entries in database_template.sql, and sets the version
void firstTimeSetup(FuzeDBI::Connection* fuze_dbi, const std::filesystem::path& template_path, const std::filesystem::path& absolute_sqlite_path, const std::string& current_version);
// Returns true if any migrations need to be made by psql
bool writeMigrations(std::ostream& stream, const std::string& database_version_string);
// void writeNewMigrations(FuzeDBI::Connection* fuze_dbi, Fuze::MigrationHelper::Migrations& migrater);
// Wrapper for writeMigrations
void makeMigrations(FuzeDBI::Connection* fuze_dbi, const std::string& database_version_string, const std::string& current_version);
} // Migrations
} // FuzeHttp
