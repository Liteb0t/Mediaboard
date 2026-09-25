// Copyright (c) 2026, Fuze.page
// Fuze Human-oriented License v1
module;
#include <list>
#include <memory>
#include <print>
module Mediaboard.State;

import FuzeHttp.Migrations;

namespace Mediaboard {

void State::addMigrations() {
	this->migrations.addMigrations()
	("0.1.1", "ALTER TABLE message_file ADD COLUMN width INTEGER;"
		"ALTER TABLE message_file ADD COLUMN height INTEGER;")
	("0.1.2", "ALTER TABLE message_file ADD COLUMN thumbnail_file_extension TEXT;"
		"ALTER TABLE thread ADD COLUMN message_id_seq INTEGER DEFAULT 0;"
		"UPDATE thread SET message_id_seq = 1000")
	("0.2", this, [](FuzeDBI::Connection* db, Mediaboard::State* state){
		std::println("This is the lambda and document_root is {}", state->getDocumentRoot().string()); })
	("0.2", "CREATE TABLE board(id INTEGER PRIMARY KEY, slug TEXT, title TEXT, thread_id_seq INTEGER DEFAULT 0, permission_object_id INTEGER, deleted BOOLEAN DEFAULT FALSE);")
	("0.2", "INSERT INTO board(id, slug, title, permission_object_id) VALUES (0, 'board', 'Board', 1);")
	("0.2", "ALTER TABLE thread ADD COLUMN board_id INTEGER DEFAULT 0;")
	("0.2", "ALTER TABLE _sequences ADD COLUMN board_id INTEGER DEFAULT 1;")
	("0.2", "CREATE TABLE ice_servers(type TEXT, hostname TEXT, port INTEGER, transport TEXT, shared_secret TEXT)")
	;
	this->migrations.add("0.2.1", "ALTER TABLE message ADD COLUMN highest_ranked_group_name TEXT");
}
}
