#pragma once
#include "FuzeDBI.hpp"
#include "shared_state.hpp"
// #include "FuzeMigrationHelper.hpp"
#include <boost/filesystem/path.hpp>
#include <iostream>
#include <string>

const std::string current_version = "0.1.3";

namespace Migrations {
// Populates database with entries in database_template.sql, and sets the version
void firstTimeSetup(FuzeDBI::Connection* fuze_dbi, const std::filesystem::path& template_path, const std::filesystem::path& absolute_sqlite_path);
// Returns true if any migrations need to be made by psql
bool writeMigrations(std::ostream& stream, const std::string& database_version_string, const StateConfig& state_config);
// void writeNewMigrations(FuzeDBI::Connection* fuze_dbi, Fuze::MigrationHelper::Migrations& migrater);
// Wrapper for writeMigrations
void makeMigrations(FuzeDBI::Connection* fuze_dbi, const std::string& database_version_string, const StateConfig& state_config);
}
