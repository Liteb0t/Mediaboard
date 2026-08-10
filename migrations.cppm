module;
#include "FuzeDBI.hpp"
#include <list>
#include <memory>
#include <print>
export module Mediaboard.Migrations;

import FuzeHttp.Migrations;
import Mediaboard.State;

using namespace FuzeHttp::Migrations;

export std::list<std::unique_ptr<Migration>> addMigrations( Mediaboard::State* state) {
	std::list<std::unique_ptr<Migration>> migrations;
	migrations.push_back(std::unique_ptr<Migration>(new SQLOnlyMigration("0.1.1", "ALTER TABLE message_file ADD COLUMN width INTEGER;"
	"ALTER TABLE message_file ADD COLUMN height INTEGER;")));
	migrations.push_back(std::unique_ptr<Migration>(new SQLOnlyMigration("0.1.2", "ALTER TABLE message_file ADD COLUMN thumbnail_file_extension TEXT;"
	"ALTER TABLE thread ADD COLUMN message_id_seq INTEGER DEFAULT 0;"
	"UPDATE thread SET message_id_seq = 1000")));
	// migrations.push_back(std::unique_ptr<Migration>(new SQLOnlyMigration("0.2.2", "CREATE TABLE sql_test(ting TEXT)")));
	migrations.push_back(std::unique_ptr<Migration>(new SmartMigration("0.2", state,
		[](FuzeDBI::Connection* db, Mediaboard::State* state){std::println("This is the lambda and document_root is {}", state->getDocumentRoot().string()); })));
	return migrations;
}
