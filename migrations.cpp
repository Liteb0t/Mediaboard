module;
#include <list>
#include <memory>
#include <print>
module Mediaboard.State;

import FuzeHttp.Migrations;

using namespace FuzeHttp::Migrations;

namespace Mediaboard {

std::list<std::unique_ptr<Migration>> State::addMigrations() {
	std::list<std::unique_ptr<Migration>> migrations;
	migrations.push_back(std::unique_ptr<Migration>(new SQLOnlyMigration("0.1.1", "ALTER TABLE message_file ADD COLUMN width INTEGER;"
	"ALTER TABLE message_file ADD COLUMN height INTEGER;")));
	migrations.push_back(std::unique_ptr<Migration>(new SQLOnlyMigration("0.1.2", "ALTER TABLE message_file ADD COLUMN thumbnail_file_extension TEXT;"
	"ALTER TABLE thread ADD COLUMN message_id_seq INTEGER DEFAULT 0;"
	"UPDATE thread SET message_id_seq = 1000")));
	// migrations.push_back(std::unique_ptr<Migration>(new SQLOnlyMigration("0.2.2", "CREATE TABLE sql_test(ting TEXT)")));
	// migrations.push_back(std::unique_ptr<Migration>(new SmartMigration("0.2", this, [](FuzeDBI::Connection* db, Mediaboard::State* state){
	// 	std::println("This is the lambda and document_root is {}", state->getDocumentRoot().string()); })));
	migrations.push_back(std::unique_ptr<Migration>(new SQLOnlyMigration("0.2",
		"CREATE TABLE board(id INTEGER PRIMARY KEY, slug TEXT, title TEXT, thread_id_seq INTEGER DEFAULT 0, permission_object_id INTEGER);")));
	migrations.push_back(std::unique_ptr<Migration>(new SQLOnlyMigration("0.2",
		"INSERT INTO board(id, slug, title, permission_object_id) VALUES (0, 'board', 'Board', 1);")));
	migrations.push_back(std::unique_ptr<Migration>(new SQLOnlyMigration("0.2",
		"ALTER TABLE thread ADD COLUMN board_id INTEGER DEFAULT 0;")));
	migrations.push_back(std::unique_ptr<Migration>(new SQLOnlyMigration("0.2",
		"ALTER TABLE _sequences ADD COLUMN board_id INTEGER DEFAULT 1;")));
	// migrations.push_back(std::unique_ptr<Migration>(new SQLOnlyMigration("0.2",
	// 	"ALTER TABLE thread ADD COLUMN id_in_board INTEGER DEFAULT 0;")));
	// migrations.push_back(std::unique_ptr<Migration>(new SQLOnlyMigration("0.2",
	// 	"UPDATE thread SET id_in_board = id;")));
	// migrations.push_back(std::unique_ptr<Migration>(new SQLOnlyMigration("0.2",
	// 	"ALTER TABLE message ADD COLUMN board_id INTEGER DEFAULT 0;")));
	return migrations;
}
}
