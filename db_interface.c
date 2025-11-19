/* Processed by ecpg (15.14 (Debian 15.14-0+deb12u1)) */
/* These include files are added by the preprocessor */
#include <ecpglib.h>
#include <ecpgerrno.h>
#include <sqlca.h>
/* End of automatic include section */

#line 1 "db_interface.pgc"
#include "db_interface.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pgtypes_timestamp.h>


void print_sqlca();
/* exec sql whenever sqlerror  call print_sqlca ( ) ; */
#line 10 "db_interface.pgc"

// EXEC SQL WHENEVER SQLERROR STOP; // https://www.postgresql.org/docs/14/ecpg-errors.html

// https://stackoverflow.com/a/3536261
void initPostArray(struct db_post_array *a, size_t initialSize) {
  a->array = malloc(initialSize * sizeof(struct db_post_struct));
  a->used = 0;
  a->size = initialSize;
}

void insertToPostArray(struct db_post_array *a, struct db_post_struct element) {
  // a->used is the number of used entries, because a->array[a->used++] updates a->used only *after* the array has been accessed.
  // Therefore a->used can go up to a->size
  if (a->used == a->size) {
    a->size *= 2;
    a->array = realloc(a->array, a->size * sizeof(struct db_post_struct));
  }
  a->array[a->used++] = element;
}

void freePostArray(struct db_post_array *a) {
  free(a->array);
  a->array = NULL;
  a->used = a->size = 0;
}

// https://stackoverflow.com/a/3536261
void initThreadArray(struct db_thread_array *a, size_t initialSize) {
  a->array = malloc(initialSize * sizeof(struct db_thread_struct));
  a->used = 0;
  a->size = initialSize;
}

void insertToThreadArray(struct db_thread_array *a, struct db_thread_struct element) {
  // a->used is the number of used entries, because a->array[a->used++] updates a->used only *after* the array has been accessed.
  // Therefore a->used can go up to a->size
  if (a->used == a->size) {
    a->size *= 2;
    a->array = realloc(a->array, a->size * sizeof(struct db_thread_struct));
  }
  a->array[a->used++] = element;
}

void freeThreadArray(struct db_thread_array *a) {
  free(a->array);
  a->array = NULL;
  a->used = a->size = 0;
}

void db_connect(const char* _database_name) {
	/* exec sql begin declare section */
	    
	    
	// const char* password = "watermelone";
	    
	    
	    
	    
	 
	
#line 61 "db_interface.pgc"
 const char * database_name = _database_name ;
 
#line 62 "db_interface.pgc"
 const char * password = getenv ( DATABASE_PASSWORD_ENVIRONMENT_VARIABLE ) ;
 
#line 64 "db_interface.pgc"
 const char * upload_post_prepared_stmt = "INSERT INTO post(id, thread, id_in_thread, name, upload_timestamp, content, files[0], files[1], files[2], files[3], key) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);" ;
 
#line 65 "db_interface.pgc"
 const char * create_thread_prepared_stmt = "INSERT INTO thread(id, number_of_posts) VALUES (?, 0);" ;
 
#line 66 "db_interface.pgc"
 const char * mark_post_as_deleted_prepared_stmt = "UPDATE post SET deleted = true WHERE id = ?;" ;
 
#line 67 "db_interface.pgc"
 const char * create_account_prepared_stmt = "INSERT INTO account(username, password_hash, key) VALUES (?, ?, ?);" ;
 
#line 68 "db_interface.pgc"
 char * data ;
/* exec sql end declare section */
#line 69 "db_interface.pgc"

	// printf("password: %s\n", password);
	if (password)
		printf("Found environment variable \"%s\"\n", DATABASE_PASSWORD_ENVIRONMENT_VARIABLE);
	else {
		fprintf(stderr, "Could not find environment variable \"%s\" required for the database connection password. Exiting.", DATABASE_PASSWORD_ENVIRONMENT_VARIABLE);
		exit(2);
	}
	{ ECPGconnect(__LINE__, 0, database_name , "mediaboard_server" , password , NULL, 0); 
#line 77 "db_interface.pgc"

if (sqlca.sqlcode < 0) print_sqlca ( );}
#line 77 "db_interface.pgc"

	printf("Connected!\n");
	{ ECPGprepare(__LINE__, NULL, 0, "upload_post_stmt", upload_post_prepared_stmt);
#line 79 "db_interface.pgc"

if (sqlca.sqlcode < 0) print_sqlca ( );}
#line 79 "db_interface.pgc"

	{ ECPGprepare(__LINE__, NULL, 0, "create_thread_stmt", create_thread_prepared_stmt);
#line 80 "db_interface.pgc"

if (sqlca.sqlcode < 0) print_sqlca ( );}
#line 80 "db_interface.pgc"

	{ ECPGprepare(__LINE__, NULL, 0, "mark_post_as_deleted_stmt", mark_post_as_deleted_prepared_stmt);
#line 81 "db_interface.pgc"

if (sqlca.sqlcode < 0) print_sqlca ( );}
#line 81 "db_interface.pgc"

	{ ECPGprepare(__LINE__, NULL, 0, "create_account_prepared_stmt", create_account_prepared_stmt);
#line 82 "db_interface.pgc"

if (sqlca.sqlcode < 0) print_sqlca ( );}
#line 82 "db_interface.pgc"

}

void db_test() {
    /* exec sql begin declare section */
     
    
#line 87 "db_interface.pgc"
 char dbname [ 1024 ] ;
/* exec sql end declare section */
#line 88 "db_interface.pgc"


    { ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_normal, "select current_database ( )", ECPGt_EOIT, 
	ECPGt_char,(dbname),(long)1024,(long)1,(1024)*sizeof(char), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, ECPGt_EORT);
#line 90 "db_interface.pgc"

if (sqlca.sqlcode < 0) print_sqlca ( );}
#line 90 "db_interface.pgc"

    printf("current_database = %s\n", dbname);
}

void db_create_administrator(const char* _password) {
	/* exec sql begin declare section */
	    
	 
	 
	
#line 96 "db_interface.pgc"
 const char * password = _password ;
 
#line 97 "db_interface.pgc"
 char hashed_password [ 60 + 1 ] ;
 
#line 98 "db_interface.pgc"
 char key [ 8 + 1 ] ;
/* exec sql end declare section */
#line 99 "db_interface.pgc"

	{ ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_normal, "delete from account where username = 'Administrator'", ECPGt_EOIT, ECPGt_EORT);
#line 100 "db_interface.pgc"

if (sqlca.sqlcode < 0) print_sqlca ( );}
#line 100 "db_interface.pgc"

	{ ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_normal, "select crypt ( $1  , gen_salt ( 'bf' ) )", 
	ECPGt_char,&(password),(long)0,(long)1,(1)*sizeof(char), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, ECPGt_EOIT, 
	ECPGt_char,(hashed_password),(long)60 + 1,(long)1,(60 + 1)*sizeof(char), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, ECPGt_EORT);
#line 101 "db_interface.pgc"

if (sqlca.sqlcode < 0) print_sqlca ( );}
#line 101 "db_interface.pgc"

	strncpy(key, &hashed_password[25], 8); // Copying 8 characters from the index 25 takes 4 characters from the salt and 4 from the hash
	key[8] = '\0';
	{ ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_normal, "insert into account ( username , password_hash , key , administrator ) values ( 'Administrator' , $1  , $2  , 't' )", 
	ECPGt_char,(hashed_password),(long)60 + 1,(long)1,(60 + 1)*sizeof(char), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, 
	ECPGt_char,(key),(long)8 + 1,(long)1,(8 + 1)*sizeof(char), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, ECPGt_EOIT, ECPGt_EORT);
#line 104 "db_interface.pgc"

if (sqlca.sqlcode < 0) print_sqlca ( );}
#line 104 "db_interface.pgc"

	{ ECPGtrans(__LINE__, NULL, "commit");
#line 105 "db_interface.pgc"

if (sqlca.sqlcode < 0) print_sqlca ( );}
#line 105 "db_interface.pgc"

}

int db_store_post(int thread_id_, int id_in_thread_, const char name_[POST_MAX_NAME+1], time_t upload_timestamp_, const char* content_, char files[4][POST_MAX_FILE_NAME+1], int file_count, const char key_[KEY_LENGTH+1]) {
    /* exec sql begin declare section */
	 
	 
	   
	   
	    
	 
	    
	           
	           
	           
	           
	    
    
#line 110 "db_interface.pgc"
 int number_of_posts ;
 
#line 111 "db_interface.pgc"
 int post_id ;
 
#line 112 "db_interface.pgc"
 int thread_id = thread_id_ ;
 
#line 113 "db_interface.pgc"
 int id_in_thread = id_in_thread_ ;
 
#line 114 "db_interface.pgc"
 const char * name = name_ ;
 
#line 115 "db_interface.pgc"
 char upload_timestamp [ 20 ] ;
 
#line 116 "db_interface.pgc"
 const char * content = content_ ;
 
#line 117 "db_interface.pgc"
 const char * file_1 = files [ 0 ] ;
 
#line 117 "db_interface.pgc"
 const int file_1_indicator = file_count - 1 ;
 
#line 118 "db_interface.pgc"
 const char * file_2 = files [ 1 ] ;
 
#line 118 "db_interface.pgc"
 const int file_2_indicator = file_count - 2 ;
 
#line 119 "db_interface.pgc"
 const char * file_3 = files [ 2 ] ;
 
#line 119 "db_interface.pgc"
 const int file_3_indicator = file_count - 3 ;
 
#line 120 "db_interface.pgc"
 const char * file_4 = files [ 3 ] ;
 
#line 120 "db_interface.pgc"
 const int file_4_indicator = file_count - 4 ;
 
#line 121 "db_interface.pgc"
 const char * key = key_ ;
/* exec sql end declare section */
#line 122 "db_interface.pgc"

	{ ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_normal, "select number_of_posts from thread where id = $1 ", 
	ECPGt_int,&(thread_id),(long)1,(long)1,sizeof(int), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, ECPGt_EOIT, 
	ECPGt_int,&(number_of_posts),(long)1,(long)1,sizeof(int), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, ECPGt_EORT);
#line 123 "db_interface.pgc"

if (sqlca.sqlcode < 0) print_sqlca ( );}
#line 123 "db_interface.pgc"

	{ ECPGtrans(__LINE__, NULL, "commit");
#line 124 "db_interface.pgc"

if (sqlca.sqlcode < 0) print_sqlca ( );}
#line 124 "db_interface.pgc"

	number_of_posts++;
	{ ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_normal, "update thread set number_of_posts = $1  where id = $2 ", 
	ECPGt_int,&(number_of_posts),(long)1,(long)1,sizeof(int), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, 
	ECPGt_int,&(thread_id),(long)1,(long)1,sizeof(int), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, ECPGt_EOIT, ECPGt_EORT);
#line 128 "db_interface.pgc"

if (sqlca.sqlcode < 0) print_sqlca ( );}
#line 128 "db_interface.pgc"

	// strcpy(data, data_);
	strftime(upload_timestamp, 20, "%Y-%m-%d %H:%M:%S", gmtime(&upload_timestamp_));
	printf("[DB Interface] Storing post\nName: %s\nTimestamp: %s\nContent: %s[END]\n", name, upload_timestamp, content);
	{ ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_normal, "select nextval ( 'post_id_seq' )", ECPGt_EOIT, 
	ECPGt_int,&(post_id),(long)1,(long)1,sizeof(int), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, ECPGt_EORT);
#line 132 "db_interface.pgc"

if (sqlca.sqlcode < 0) print_sqlca ( );}
#line 132 "db_interface.pgc"

	{ ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_execute, "upload_post_stmt", 
	ECPGt_int,&(post_id),(long)1,(long)1,sizeof(int), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, 
	ECPGt_int,&(thread_id),(long)1,(long)1,sizeof(int), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, 
	ECPGt_int,&(id_in_thread),(long)1,(long)1,sizeof(int), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, 
	ECPGt_char,&(name),(long)0,(long)1,(1)*sizeof(char), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, 
	ECPGt_char,(upload_timestamp),(long)20,(long)1,(20)*sizeof(char), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, 
	ECPGt_char,&(content),(long)0,(long)1,(1)*sizeof(char), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, 
	ECPGt_char,&(file_1),(long)0,(long)1,(1)*sizeof(char), 
	ECPGt_int,&(file_1_indicator),(long)1,(long)1,sizeof(int), 
	ECPGt_char,&(file_2),(long)0,(long)1,(1)*sizeof(char), 
	ECPGt_int,&(file_2_indicator),(long)1,(long)1,sizeof(int), 
	ECPGt_char,&(file_3),(long)0,(long)1,(1)*sizeof(char), 
	ECPGt_int,&(file_3_indicator),(long)1,(long)1,sizeof(int), 
	ECPGt_char,&(file_4),(long)0,(long)1,(1)*sizeof(char), 
	ECPGt_int,&(file_4_indicator),(long)1,(long)1,sizeof(int), 
	ECPGt_char,&(key),(long)0,(long)1,(1)*sizeof(char), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, ECPGt_EOIT, ECPGt_EORT);
#line 133 "db_interface.pgc"

if (sqlca.sqlcode < 0) print_sqlca ( );}
#line 133 "db_interface.pgc"

	{ ECPGtrans(__LINE__, NULL, "commit");
#line 134 "db_interface.pgc"

if (sqlca.sqlcode < 0) print_sqlca ( );}
#line 134 "db_interface.pgc"

	return post_id;
}

int db_store_thread(int number_of_posts_) {
	printf("Executing db_store_thread\n");
	/* exec sql begin declare section */
	 
	// int number_of_posts = number_of_posts_;
	
#line 141 "db_interface.pgc"
 int thread_id ;
/* exec sql end declare section */
#line 143 "db_interface.pgc"

	{ ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_normal, "select nextval ( 'thread_id_seq' )", ECPGt_EOIT, 
	ECPGt_int,&(thread_id),(long)1,(long)1,sizeof(int), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, ECPGt_EORT);
#line 144 "db_interface.pgc"

if (sqlca.sqlcode < 0) print_sqlca ( );}
#line 144 "db_interface.pgc"

	{ ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_execute, "create_thread_stmt", 
	ECPGt_int,&(thread_id),(long)1,(long)1,sizeof(int), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, ECPGt_EOIT, ECPGt_EORT);
#line 145 "db_interface.pgc"

if (sqlca.sqlcode < 0) print_sqlca ( );}
#line 145 "db_interface.pgc"

	{ ECPGtrans(__LINE__, NULL, "commit");
#line 146 "db_interface.pgc"

if (sqlca.sqlcode < 0) print_sqlca ( );}
#line 146 "db_interface.pgc"

	return thread_id;
}

// void db_mark_message_as_deleted(int thread_id_, int post_id_) {
void db_mark_post_as_deleted(int post_id_) {
	/* exec sql begin declare section */
	// int thread_id = thread_id_;
	   
	
#line 154 "db_interface.pgc"
 int post_id = post_id_ ;
/* exec sql end declare section */
#line 155 "db_interface.pgc"

	{ ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_execute, "mark_post_as_deleted_stmt", 
	ECPGt_int,&(post_id),(long)1,(long)1,sizeof(int), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, ECPGt_EOIT, ECPGt_EORT);
#line 156 "db_interface.pgc"

if (sqlca.sqlcode < 0) print_sqlca ( );}
#line 156 "db_interface.pgc"

	{ ECPGtrans(__LINE__, NULL, "commit");
#line 157 "db_interface.pgc"

if (sqlca.sqlcode < 0) print_sqlca ( );}
#line 157 "db_interface.pgc"

}

int db_store_account(const char* _username, const char* _password) {
	/* exec sql begin declare section */
	    
	    
	 
	 
	 
	
#line 162 "db_interface.pgc"
 const char * username = _username ;
 
#line 163 "db_interface.pgc"
 const char * password = _password ;
 
#line 164 "db_interface.pgc"
 char hashed_password [ 60 + 1 ] ;
 
#line 165 "db_interface.pgc"
 char key [ 8 + 1 ] ;
 
#line 166 "db_interface.pgc"
 char username_exists ;
/* exec sql end declare section */
#line 167 "db_interface.pgc"

	printf("Calling db_store_account with username: %s, password: %s\n", username, password);
	{ ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_normal, "select exists ( select 1 from account where username = $1  limit 1 )", 
	ECPGt_char,&(username),(long)0,(long)1,(1)*sizeof(char), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, ECPGt_EOIT, 
	ECPGt_char,&(username_exists),(long)1,(long)1,(1)*sizeof(char), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, ECPGt_EORT);
#line 169 "db_interface.pgc"

if (sqlca.sqlcode < 0) print_sqlca ( );}
#line 169 "db_interface.pgc"

	{ ECPGtrans(__LINE__, NULL, "commit");
#line 170 "db_interface.pgc"

if (sqlca.sqlcode < 0) print_sqlca ( );}
#line 170 "db_interface.pgc"

	// -------------------------> block if using reserved name "Administrator"
	if (username_exists == 't' || (strcmp(username, "Administrator") == 0))
		return 1;
	else {
		{ ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_normal, "select crypt ( $1  , gen_salt ( 'bf' ) )", 
	ECPGt_char,&(password),(long)0,(long)1,(1)*sizeof(char), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, ECPGt_EOIT, 
	ECPGt_char,(hashed_password),(long)60 + 1,(long)1,(60 + 1)*sizeof(char), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, ECPGt_EORT);
#line 175 "db_interface.pgc"

if (sqlca.sqlcode < 0) print_sqlca ( );}
#line 175 "db_interface.pgc"

		{ ECPGtrans(__LINE__, NULL, "commit");
#line 176 "db_interface.pgc"

if (sqlca.sqlcode < 0) print_sqlca ( );}
#line 176 "db_interface.pgc"

		strncpy(key, &hashed_password[25], 8); // Copying 8 characters from the index 25 takes 4 characters from the salt and 4 from the hash
		printf("hashed_password: %s\n", hashed_password);
		printf("key: %s\n", key);
		{ ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_execute, "create_account_prepared_stmt", 
	ECPGt_char,&(username),(long)0,(long)1,(1)*sizeof(char), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, 
	ECPGt_char,(hashed_password),(long)60 + 1,(long)1,(60 + 1)*sizeof(char), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, 
	ECPGt_char,(key),(long)8 + 1,(long)1,(8 + 1)*sizeof(char), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, ECPGt_EOIT, ECPGt_EORT);
#line 180 "db_interface.pgc"

if (sqlca.sqlcode < 0) print_sqlca ( );}
#line 180 "db_interface.pgc"

		{ ECPGtrans(__LINE__, NULL, "commit");
#line 181 "db_interface.pgc"

if (sqlca.sqlcode < 0) print_sqlca ( );}
#line 181 "db_interface.pgc"

		return 0;
	}
}

const char db_fetch_key(char* _key, const char* _username, const char* _password) {
	/* exec sql begin declare section */
	    
	    
	 
	 
	 
	
#line 188 "db_interface.pgc"
 const char * username = _username ;
 
#line 189 "db_interface.pgc"
 const char * password = _password ;
 
#line 190 "db_interface.pgc"
 char key [ 8 + 1 ] ;
 
#line 191 "db_interface.pgc"
 char hashed_password [ 61 ] ;
 
#line 192 "db_interface.pgc"
 char passwords_match ;
/* exec sql end declare section */
#line 193 "db_interface.pgc"

	printf("Calling db_fetch_key with username: %s, password: %s\n", username, password);
	hashed_password[0] = '\0';
	{ ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_normal, "select password_hash from account where username = $1 ", 
	ECPGt_char,&(username),(long)0,(long)1,(1)*sizeof(char), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, ECPGt_EOIT, 
	ECPGt_char,(hashed_password),(long)61,(long)1,(61)*sizeof(char), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, ECPGt_EORT);
#line 196 "db_interface.pgc"

if (sqlca.sqlcode < 0) print_sqlca ( );}
#line 196 "db_interface.pgc"

	{ ECPGtrans(__LINE__, NULL, "commit");
#line 197 "db_interface.pgc"

if (sqlca.sqlcode < 0) print_sqlca ( );}
#line 197 "db_interface.pgc"

	if (hashed_password[0] == '\0')
		return '\0';
	else {
		printf("Username exists. hashed_password: %s\n", hashed_password);
		{ ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_normal, "select ( $1  = crypt ( $2  , $3  ) )", 
	ECPGt_char,(hashed_password),(long)61,(long)1,(61)*sizeof(char), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, 
	ECPGt_char,&(password),(long)0,(long)1,(1)*sizeof(char), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, 
	ECPGt_char,(hashed_password),(long)61,(long)1,(61)*sizeof(char), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, ECPGt_EOIT, 
	ECPGt_char,&(passwords_match),(long)1,(long)1,(1)*sizeof(char), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, ECPGt_EORT);
#line 202 "db_interface.pgc"

if (sqlca.sqlcode < 0) print_sqlca ( );}
#line 202 "db_interface.pgc"

		if (passwords_match == 't') {
			{ ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_normal, "select key from account where username = $1 ", 
	ECPGt_char,&(username),(long)0,(long)1,(1)*sizeof(char), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, ECPGt_EOIT, 
	ECPGt_char,(key),(long)8 + 1,(long)1,(8 + 1)*sizeof(char), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, ECPGt_EORT);
#line 204 "db_interface.pgc"

if (sqlca.sqlcode < 0) print_sqlca ( );}
#line 204 "db_interface.pgc"

			{ ECPGtrans(__LINE__, NULL, "commit");
#line 205 "db_interface.pgc"

if (sqlca.sqlcode < 0) print_sqlca ( );}
#line 205 "db_interface.pgc"

			strcpy(_key, key); // Copy fetched key into the pointer passed to db_fetch_key
		}
		{ ECPGtrans(__LINE__, NULL, "commit");
#line 208 "db_interface.pgc"

if (sqlca.sqlcode < 0) print_sqlca ( );}
#line 208 "db_interface.pgc"

		return passwords_match;
	}
}

int db_key_matches_account(const char* _key, const char* _username) {
	/* exec sql begin declare section */
	    
	 
	 
	
#line 215 "db_interface.pgc"
 const char * username = _username ;
 
#line 216 "db_interface.pgc"
 char key [ 8 + 1 ] ;
 
#line 217 "db_interface.pgc"
 char username_exists ;
/* exec sql end declare section */
#line 218 "db_interface.pgc"

	printf("Calling db_key_matches_account with username: %s, key: %s\n", username, _key);
	{ ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_normal, "select exists ( select 1 from account where username = $1  limit 1 )", 
	ECPGt_char,&(username),(long)0,(long)1,(1)*sizeof(char), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, ECPGt_EOIT, 
	ECPGt_char,&(username_exists),(long)1,(long)1,(1)*sizeof(char), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, ECPGt_EORT);
#line 220 "db_interface.pgc"

if (sqlca.sqlcode < 0) print_sqlca ( );}
#line 220 "db_interface.pgc"

	{ ECPGtrans(__LINE__, NULL, "commit");
#line 221 "db_interface.pgc"

if (sqlca.sqlcode < 0) print_sqlca ( );}
#line 221 "db_interface.pgc"

	if (username_exists == 't') {
		{ ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_normal, "select key from account where username = $1 ", 
	ECPGt_char,&(username),(long)0,(long)1,(1)*sizeof(char), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, ECPGt_EOIT, 
	ECPGt_char,(key),(long)8 + 1,(long)1,(8 + 1)*sizeof(char), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, ECPGt_EORT);
#line 223 "db_interface.pgc"

if (sqlca.sqlcode < 0) print_sqlca ( );}
#line 223 "db_interface.pgc"

		{ ECPGtrans(__LINE__, NULL, "commit");
#line 224 "db_interface.pgc"

if (sqlca.sqlcode < 0) print_sqlca ( );}
#line 224 "db_interface.pgc"

		if (strcmp(key, _key) == 0)
			return 1;
		else
			return 0;
	}
	else
		return 0;
}

char db_change_password(const char* _username, const char* _old_password, const char* _new_password) {
	/* exec sql begin declare section */
	    
	    
	    
	 
	 
	 
	
#line 236 "db_interface.pgc"
 const char * username = _username ;
 
#line 237 "db_interface.pgc"
 const char * old_password = _old_password ;
 
#line 238 "db_interface.pgc"
 const char * new_password = _new_password ;
 
#line 239 "db_interface.pgc"
 char hashed_old_password [ 61 ] ;
 
#line 240 "db_interface.pgc"
 char hashed_new_password [ 61 ] ;
 
#line 241 "db_interface.pgc"
 char passwords_match ;
/* exec sql end declare section */
#line 242 "db_interface.pgc"

	printf("Calling db_change_password with:\nusername: %s\nold_password: %s\nnew_password: %s\n", username, old_password, new_password);
	hashed_old_password[0] = '\0';
	{ ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_normal, "select password_hash from account where username = $1 ", 
	ECPGt_char,&(username),(long)0,(long)1,(1)*sizeof(char), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, ECPGt_EOIT, 
	ECPGt_char,(hashed_old_password),(long)61,(long)1,(61)*sizeof(char), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, ECPGt_EORT);
#line 245 "db_interface.pgc"

if (sqlca.sqlcode < 0) print_sqlca ( );}
#line 245 "db_interface.pgc"

	{ ECPGtrans(__LINE__, NULL, "commit");
#line 246 "db_interface.pgc"

if (sqlca.sqlcode < 0) print_sqlca ( );}
#line 246 "db_interface.pgc"

	if (hashed_old_password[0] == '\0')
		return '\0';
	else {
		printf("Username exists. hashed_password: %s\n", hashed_old_password);
		{ ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_normal, "select ( $1  = crypt ( $2  , $3  ) )", 
	ECPGt_char,(hashed_old_password),(long)61,(long)1,(61)*sizeof(char), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, 
	ECPGt_char,&(old_password),(long)0,(long)1,(1)*sizeof(char), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, 
	ECPGt_char,(hashed_old_password),(long)61,(long)1,(61)*sizeof(char), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, ECPGt_EOIT, 
	ECPGt_char,&(passwords_match),(long)1,(long)1,(1)*sizeof(char), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, ECPGt_EORT);
#line 251 "db_interface.pgc"

if (sqlca.sqlcode < 0) print_sqlca ( );}
#line 251 "db_interface.pgc"

		{ ECPGtrans(__LINE__, NULL, "commit");
#line 252 "db_interface.pgc"

if (sqlca.sqlcode < 0) print_sqlca ( );}
#line 252 "db_interface.pgc"

		if (passwords_match == 't') {
			{ ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_normal, "select crypt ( $1  , gen_salt ( 'bf' ) )", 
	ECPGt_char,&(new_password),(long)0,(long)1,(1)*sizeof(char), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, ECPGt_EOIT, 
	ECPGt_char,(hashed_new_password),(long)61,(long)1,(61)*sizeof(char), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, ECPGt_EORT);
#line 254 "db_interface.pgc"

if (sqlca.sqlcode < 0) print_sqlca ( );}
#line 254 "db_interface.pgc"

			{ ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_normal, "update account set password_hash = $1  where username = $2 ", 
	ECPGt_char,(hashed_new_password),(long)61,(long)1,(61)*sizeof(char), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, 
	ECPGt_char,&(username),(long)0,(long)1,(1)*sizeof(char), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, ECPGt_EOIT, ECPGt_EORT);
#line 255 "db_interface.pgc"

if (sqlca.sqlcode < 0) print_sqlca ( );}
#line 255 "db_interface.pgc"

		}
		{ ECPGtrans(__LINE__, NULL, "commit");
#line 257 "db_interface.pgc"

if (sqlca.sqlcode < 0) print_sqlca ( );}
#line 257 "db_interface.pgc"

		return passwords_match;
	}
}

struct db_thread_array* db_retrieve_threads() {
	static struct db_thread_array thread_list;
	initThreadArray(&thread_list, 25);
	/* exec sql begin declare section */
	 
	 
	
#line 266 "db_interface.pgc"
 int thread_id ;
 
#line 267 "db_interface.pgc"
 int number_of_posts ;
/* exec sql end declare section */
#line 268 "db_interface.pgc"

	/* declare thread_getter cursor for select id , number_of_posts from thread order by id */
#line 271 "db_interface.pgc"

	{ ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_normal, "declare thread_getter cursor for select id , number_of_posts from thread order by id", ECPGt_EOIT, ECPGt_EORT);
#line 272 "db_interface.pgc"

if (sqlca.sqlcode < 0) print_sqlca ( );}
#line 272 "db_interface.pgc"

	/* exec sql whenever not found  break ; */
#line 273 "db_interface.pgc"
 // Exits the while loop when `thread_getter` has retreived all threads
	while (true) {
		{ ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_normal, "fetch thread_getter", ECPGt_EOIT, 
	ECPGt_int,&(thread_id),(long)1,(long)1,sizeof(int), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, 
	ECPGt_int,&(number_of_posts),(long)1,(long)1,sizeof(int), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, ECPGt_EORT);
#line 275 "db_interface.pgc"

if (sqlca.sqlcode == ECPG_NOT_FOUND) break;
#line 275 "db_interface.pgc"

if (sqlca.sqlcode < 0) print_sqlca ( );}
#line 275 "db_interface.pgc"

		struct db_thread_struct thread;
		thread.id = thread_id;
		thread.number_of_posts = number_of_posts;
		insertToThreadArray(&thread_list, thread);
	}
	{ ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_normal, "close thread_getter", ECPGt_EOIT, ECPGt_EORT);
#line 281 "db_interface.pgc"

if (sqlca.sqlcode < 0) print_sqlca ( );}
#line 281 "db_interface.pgc"

	{ ECPGtrans(__LINE__, NULL, "commit");
#line 282 "db_interface.pgc"

if (sqlca.sqlcode < 0) print_sqlca ( );}
#line 282 "db_interface.pgc"

	return &thread_list;
}

// Runs on start-up. Reads every message from the database and inserts it into a list of db_post_struct.
struct db_post_array* db_retrieve_history() {
	static struct db_post_array post_history;
	initPostArray(&post_history, 25);
	/* exec sql begin declare section */
	 
	 
	 
	  
	 
	 
	 
	   
	 
	 
	
#line 291 "db_interface.pgc"
 int post_id ;
 
#line 292 "db_interface.pgc"
 int thread_id ;
 
#line 293 "db_interface.pgc"
 int id_in_thread ;
 
#line 294 "db_interface.pgc"
 unsigned int upload_epoch ;
 
#line 295 "db_interface.pgc"
  struct varchar_1  { int len; char arr[ POST_MAX_NAME + 1 ]; }  post_name ;
 
#line 296 "db_interface.pgc"
 timestamp upload_timestamp ;
 
#line 297 "db_interface.pgc"
  struct varchar_2  { int len; char arr[ POST_MAX_CONTENT + 1 ]; }  post_content ;
 
#line 298 "db_interface.pgc"
 char files [ 4 ] [ POST_MAX_FILE_NAME + 1 ] ;
 
#line 298 "db_interface.pgc"
 int file_indicators [ 4 ] ;
 
#line 299 "db_interface.pgc"
 char deleted ;
 
#line 300 "db_interface.pgc"
 char key [ KEY_LENGTH + 1 ] ;
/* exec sql end declare section */
#line 301 "db_interface.pgc"

	/* declare post_getter cursor for select id , thread , id_in_thread , name , upload_timestamp , content , files [ 0 ] , files [ 1 ] , files [ 2 ] , files [ 3 ] , extract ( epoch from upload_timestamp ) :: int as upload_timestamp_int , deleted , key from post order by id */
#line 304 "db_interface.pgc"

	{ ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_normal, "declare post_getter cursor for select id , thread , id_in_thread , name , upload_timestamp , content , files [ 0 ] , files [ 1 ] , files [ 2 ] , files [ 3 ] , extract ( epoch from upload_timestamp ) :: int as upload_timestamp_int , deleted , key from post order by id", ECPGt_EOIT, ECPGt_EORT);
#line 305 "db_interface.pgc"

if (sqlca.sqlcode < 0) print_sqlca ( );}
#line 305 "db_interface.pgc"

	/* exec sql whenever not found  break ; */
#line 306 "db_interface.pgc"
 // Exits the while loop when `post_getter` has retreived all posts
	while (true) {
		{ ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_normal, "fetch post_getter", ECPGt_EOIT, 
	ECPGt_int,&(post_id),(long)1,(long)1,sizeof(int), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, 
	ECPGt_int,&(thread_id),(long)1,(long)1,sizeof(int), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, 
	ECPGt_int,&(id_in_thread),(long)1,(long)1,sizeof(int), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, 
	ECPGt_varchar,&(post_name),(long)POST_MAX_NAME + 1,(long)1,sizeof(struct varchar_1), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, 
	ECPGt_timestamp,&(upload_timestamp),(long)1,(long)1,sizeof(timestamp), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, 
	ECPGt_varchar,&(post_content),(long)POST_MAX_CONTENT + 1,(long)1,sizeof(struct varchar_2), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, 
	ECPGt_char,(files[0]),(long)POST_MAX_FILE_NAME + 1,(long)1,(POST_MAX_FILE_NAME + 1)*sizeof(char), 
	ECPGt_int,&(file_indicators[0]),(long)1,(long)1,sizeof(int), 
	ECPGt_char,(files[1]),(long)POST_MAX_FILE_NAME + 1,(long)1,(POST_MAX_FILE_NAME + 1)*sizeof(char), 
	ECPGt_int,&(file_indicators[1]),(long)1,(long)1,sizeof(int), 
	ECPGt_char,(files[2]),(long)POST_MAX_FILE_NAME + 1,(long)1,(POST_MAX_FILE_NAME + 1)*sizeof(char), 
	ECPGt_int,&(file_indicators[2]),(long)1,(long)1,sizeof(int), 
	ECPGt_char,(files[3]),(long)POST_MAX_FILE_NAME + 1,(long)1,(POST_MAX_FILE_NAME + 1)*sizeof(char), 
	ECPGt_int,&(file_indicators[3]),(long)1,(long)1,sizeof(int), 
	ECPGt_unsigned_int,&(upload_epoch),(long)1,(long)1,sizeof(unsigned int), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, 
	ECPGt_char,&(deleted),(long)1,(long)1,(1)*sizeof(char), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, 
	ECPGt_char,(key),(long)KEY_LENGTH + 1,(long)1,(KEY_LENGTH + 1)*sizeof(char), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, ECPGt_EORT);
#line 308 "db_interface.pgc"

if (sqlca.sqlcode == ECPG_NOT_FOUND) break;
#line 308 "db_interface.pgc"

if (sqlca.sqlcode < 0) print_sqlca ( );}
#line 308 "db_interface.pgc"

		struct db_post_struct post;
		post.id = post_id;
		post.thread_id = thread_id;
		post.id_in_thread = id_in_thread;
		post.upload_timestamp = (time_t)upload_epoch;
		strcpy(post.name, post_name.arr);
		post.content = malloc(sizeof(char) * post_content.len + 1);
		strcpy(post.content, post_content.arr);
		int file_i;
		for (file_i = 0; file_i < 4; file_i++) {
			if (file_indicators[file_i] >= 0) {
				strcpy(post.files[file_i], files[file_i]);
			}
			else
				break;
		}
		post.number_of_files = file_i;
		post.deleted = deleted == 't'; // In Postgres 't' means true
		strcpy(post.key, key);
		insertToPostArray(&post_history, post);
	}
	{ ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_normal, "close post_getter", ECPGt_EOIT, ECPGt_EORT);
#line 330 "db_interface.pgc"

if (sqlca.sqlcode < 0) print_sqlca ( );}
#line 330 "db_interface.pgc"

	{ ECPGtrans(__LINE__, NULL, "commit");
#line 331 "db_interface.pgc"

if (sqlca.sqlcode < 0) print_sqlca ( );}
#line 331 "db_interface.pgc"

	return (&post_history);
}

void db_disconnect(void) {
	{ ECPGdisconnect(__LINE__, "ALL");
#line 336 "db_interface.pgc"

if (sqlca.sqlcode < 0) print_sqlca ( );}
#line 336 "db_interface.pgc"

}

void print_sqlca() {
    fprintf(stderr, "==== sqlce ====\n");
    fprintf(stderr, "sqlcode: %ld\n", sqlca.sqlcode);
    fprintf(stderr, "sqlerrm.sqlerrml: %d\n", sqlca.sqlerrm.sqlerrml);
    fprintf(stderr, "sqlerrm.sqlerrmc: %s\n", sqlca.sqlerrm.sqlerrmc);
    fprintf(stderr, "sqlerrd: %ld %ld %ld %ld %ld %ld\n", sqlca.sqlerrd[0],sqlca.sqlerrd[1],sqlca.sqlerrd[2],
                                                          sqlca.sqlerrd[3],sqlca.sqlerrd[4],sqlca.sqlerrd[5]);
    fprintf(stderr, "sqlwarn: %d %d %d %d %d %d %d %d\n", sqlca.sqlwarn[0], sqlca.sqlwarn[1], sqlca.sqlwarn[2],
                                                          sqlca.sqlwarn[3], sqlca.sqlwarn[4], sqlca.sqlwarn[5],
                                                          sqlca.sqlwarn[6], sqlca.sqlwarn[7]);
    fprintf(stderr, "sqlstate: %5s\n", sqlca.sqlstate);
    fprintf(stderr, "===============\n");
	printf("Terminating the program...\n");
	exit(1); // Stop the server when an SQL error is encountered. This is likely due to incorrect setup.
}
