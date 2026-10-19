// Copyright (c) 2026, Fuze.page
// Fuze Human-oriented License v1
module;
#include <chrono>
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
	this->migrations.addMigrations()
	("0.3", "ALTER TABLE message_file RENAME TO stored_file")
	("0.3", "ALTER TABLE stored_file ADD COLUMN id INTEGER")
	("0.3", "ALTER TABLE stored_file RENAME COLUMN message_id TO coupled_post_id")
	("0.3", "ALTER TABLE stored_file RENAME COLUMN file_name TO filename")
	("0.3", "ALTER TABLE stored_file ADD COLUMN coupled_board_id INTEGER")
	("0.3", "ALTER TABLE stored_file ADD COLUMN created_at INTEGER")
	("0.3", "ALTER TABLE stored_file ADD COLUMN deleted_at INTEGER")
	("0.3", "ALTER TABLE _sequences ADD COLUMN stored_file_id INTEGER DEFAULT 0")
	("0.3", "ALTER TABLE stored_file ADD COLUMN permission_object_id INTEGER")
	("0.3", "ALTER TABLE stored_file ADD COLUMN status INTEGER DEFAULT 0")
	("0.3", this, [](FuzeDBI::Connection* db, Mediaboard::State* state){
		for (std::string filename : db->queryRows<std::string>("SELECT filename FROM stored_file")) {
			int file_id = db->incrementSequence("stored_file_id");
			int permission_object_id = db->incrementSequence("permission_object_id");
			db->query<void>("UPDATE stored_file SET id = $1 WHERE filename = $2", file_id, filename);
			std::chrono::time_point<std::chrono::system_clock> created_at = std::chrono::system_clock::now();
			int time_since_epoch = std::chrono::duration_cast<std::chrono::seconds>(created_at.time_since_epoch()).count();
			db->query<void>("UPDATE stored_file SET created_at = $1 WHERE id = $2", time_since_epoch, file_id);
			db->query<void>("UPDATE stored_file SET permission_object_id = $1 WHERE id = $2", permission_object_id, file_id);

			int coupled_post_id = db->query<int>("SELECT coupled_post_id FROM stored_file WHERE id = $1", file_id);
			std::print("Fetched post {} from file {}", coupled_post_id, file_id);
			int thread_id = db->query<int>("SELECT thread_id FROM message WHERE id = $1", coupled_post_id);
			std::print("Fetched thread {} from message {}", thread_id, coupled_post_id);
			int board_id = db->query<int>("SELECT board_id FROM thread WHERE id = $1", thread_id);
			std::print("Fetched board {} from thread {}", board_id, thread_id);
			db->query<void>("UPDATE stored_file SET coupled_board_id = $1 WHERE id = $2", board_id, file_id);
		}
	})
	("0.3", "CREATE TABLE file_view(file_id INTEGER PRIMARY KEY, viewed_at INTEGER NOT NULL, client_id INTEGER)")
	;
}
}
