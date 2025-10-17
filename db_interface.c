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

void db_connect() {
	/* exec sql begin declare section */
	    
	// const char* password = "watermelone";
	    
	    
	    
	 
	
#line 61 "db_interface.pgc"
 const char * password = getenv ( DATABASE_PASSWORD_ENVIRONMENT_VARIABLE ) ;
 
#line 63 "db_interface.pgc"
 const char * upload_post_prepared_stmt = "INSERT INTO post(id, thread, id_in_thread, name, upload_timestamp, content, files[0], files[1], files[2], files[3], key) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);" ;
 
#line 64 "db_interface.pgc"
 const char * create_thread_prepared_stmt = "INSERT INTO thread(id, number_of_posts) VALUES (?, 0);" ;
 
#line 65 "db_interface.pgc"
 const char * mark_post_as_deleted_prepared_stmt = "UPDATE post SET deleted = true WHERE id = ?;" ;
 
#line 66 "db_interface.pgc"
 char * data ;
/* exec sql end declare section */
#line 67 "db_interface.pgc"

	// printf("password: %s\n", password);
	if (password)
		printf("Found environment variable \"%s\"\n", DATABASE_PASSWORD_ENVIRONMENT_VARIABLE);
	else {
		fprintf(stderr, "Could not find environment variable \"%s\" required for the database connection password. Exiting.", DATABASE_PASSWORD_ENVIRONMENT_VARIABLE);
		exit(2);
	}
	{ ECPGconnect(__LINE__, 0, "fuze_mediaboard" , "mediaboard_server" , password , NULL, 0); 
#line 75 "db_interface.pgc"

if (sqlca.sqlcode < 0) print_sqlca ( );}
#line 75 "db_interface.pgc"

	printf("Connected!\n");
	{ ECPGprepare(__LINE__, NULL, 0, "upload_post_stmt", upload_post_prepared_stmt);
#line 77 "db_interface.pgc"

if (sqlca.sqlcode < 0) print_sqlca ( );}
#line 77 "db_interface.pgc"

	{ ECPGprepare(__LINE__, NULL, 0, "create_thread_stmt", create_thread_prepared_stmt);
#line 78 "db_interface.pgc"

if (sqlca.sqlcode < 0) print_sqlca ( );}
#line 78 "db_interface.pgc"

	{ ECPGprepare(__LINE__, NULL, 0, "mark_post_as_deleted_stmt", mark_post_as_deleted_prepared_stmt);
#line 79 "db_interface.pgc"

if (sqlca.sqlcode < 0) print_sqlca ( );}
#line 79 "db_interface.pgc"

}

void db_test() {
    /* exec sql begin declare section */
     
    
#line 84 "db_interface.pgc"
 char dbname [ 1024 ] ;
/* exec sql end declare section */
#line 85 "db_interface.pgc"


    { ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_normal, "select current_database ( )", ECPGt_EOIT, 
	ECPGt_char,(dbname),(long)1024,(long)1,(1024)*sizeof(char), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, ECPGt_EORT);
#line 87 "db_interface.pgc"

if (sqlca.sqlcode < 0) print_sqlca ( );}
#line 87 "db_interface.pgc"

    printf("current_database = %s\n", dbname);
}

int db_store_post(int thread_id_, int id_in_thread_, const char name_[POST_MAX_NAME+1], time_t upload_timestamp_, const char* content_, char files[4][POST_MAX_FILE_NAME+1], int file_count, const char key_[KEY_LENGTH+1]) {
    /* exec sql begin declare section */
	 
	 
	   
	   
	    
	 
	    
	           
	           
	           
	           
	    
    
#line 93 "db_interface.pgc"
 int number_of_posts ;
 
#line 94 "db_interface.pgc"
 int post_id ;
 
#line 95 "db_interface.pgc"
 int thread_id = thread_id_ ;
 
#line 96 "db_interface.pgc"
 int id_in_thread = id_in_thread_ ;
 
#line 97 "db_interface.pgc"
 const char * name = name_ ;
 
#line 98 "db_interface.pgc"
 char upload_timestamp [ 20 ] ;
 
#line 99 "db_interface.pgc"
 const char * content = content_ ;
 
#line 100 "db_interface.pgc"
 const char * file_1 = files [ 0 ] ;
 
#line 100 "db_interface.pgc"
 const int file_1_indicator = file_count - 1 ;
 
#line 101 "db_interface.pgc"
 const char * file_2 = files [ 1 ] ;
 
#line 101 "db_interface.pgc"
 const int file_2_indicator = file_count - 2 ;
 
#line 102 "db_interface.pgc"
 const char * file_3 = files [ 2 ] ;
 
#line 102 "db_interface.pgc"
 const int file_3_indicator = file_count - 3 ;
 
#line 103 "db_interface.pgc"
 const char * file_4 = files [ 3 ] ;
 
#line 103 "db_interface.pgc"
 const int file_4_indicator = file_count - 4 ;
 
#line 104 "db_interface.pgc"
 const char * key = key_ ;
/* exec sql end declare section */
#line 105 "db_interface.pgc"

	{ ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_normal, "select number_of_posts from thread where id = $1 ", 
	ECPGt_int,&(thread_id),(long)1,(long)1,sizeof(int), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, ECPGt_EOIT, 
	ECPGt_int,&(number_of_posts),(long)1,(long)1,sizeof(int), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, ECPGt_EORT);
#line 106 "db_interface.pgc"

if (sqlca.sqlcode < 0) print_sqlca ( );}
#line 106 "db_interface.pgc"

	{ ECPGtrans(__LINE__, NULL, "commit");
#line 107 "db_interface.pgc"

if (sqlca.sqlcode < 0) print_sqlca ( );}
#line 107 "db_interface.pgc"

	number_of_posts++;
	{ ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_normal, "update thread set number_of_posts = $1  where id = $2 ", 
	ECPGt_int,&(number_of_posts),(long)1,(long)1,sizeof(int), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, 
	ECPGt_int,&(thread_id),(long)1,(long)1,sizeof(int), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, ECPGt_EOIT, ECPGt_EORT);
#line 111 "db_interface.pgc"

if (sqlca.sqlcode < 0) print_sqlca ( );}
#line 111 "db_interface.pgc"

	// strcpy(data, data_);
	strftime(upload_timestamp, 20, "%Y-%m-%d %H:%M:%S", gmtime(&upload_timestamp_));
	printf("[DB Interface] Storing post\nName: %s\nTimestamp: %s\nContent: %s[END]\n", name, upload_timestamp, content);
	{ ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_normal, "select nextval ( 'post_id_seq' )", ECPGt_EOIT, 
	ECPGt_int,&(post_id),(long)1,(long)1,sizeof(int), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, ECPGt_EORT);
#line 115 "db_interface.pgc"

if (sqlca.sqlcode < 0) print_sqlca ( );}
#line 115 "db_interface.pgc"

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
#line 116 "db_interface.pgc"

if (sqlca.sqlcode < 0) print_sqlca ( );}
#line 116 "db_interface.pgc"

	{ ECPGtrans(__LINE__, NULL, "commit");
#line 117 "db_interface.pgc"

if (sqlca.sqlcode < 0) print_sqlca ( );}
#line 117 "db_interface.pgc"

	return post_id;
}

int db_store_thread(int number_of_posts_) {
	printf("Executing db_store_thread\n");
	/* exec sql begin declare section */
	 
	// int number_of_posts = number_of_posts_;
	
#line 124 "db_interface.pgc"
 int thread_id ;
/* exec sql end declare section */
#line 126 "db_interface.pgc"

	{ ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_normal, "select nextval ( 'thread_id_seq' )", ECPGt_EOIT, 
	ECPGt_int,&(thread_id),(long)1,(long)1,sizeof(int), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, ECPGt_EORT);
#line 127 "db_interface.pgc"

if (sqlca.sqlcode < 0) print_sqlca ( );}
#line 127 "db_interface.pgc"

	{ ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_execute, "create_thread_stmt", 
	ECPGt_int,&(thread_id),(long)1,(long)1,sizeof(int), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, ECPGt_EOIT, ECPGt_EORT);
#line 128 "db_interface.pgc"

if (sqlca.sqlcode < 0) print_sqlca ( );}
#line 128 "db_interface.pgc"

	{ ECPGtrans(__LINE__, NULL, "commit");
#line 129 "db_interface.pgc"

if (sqlca.sqlcode < 0) print_sqlca ( );}
#line 129 "db_interface.pgc"

	return thread_id;
}

// void db_mark_message_as_deleted(int thread_id_, int post_id_) {
void db_mark_post_as_deleted(int post_id_) {
	/* exec sql begin declare section */
	// int thread_id = thread_id_;
	   
	
#line 137 "db_interface.pgc"
 int post_id = post_id_ ;
/* exec sql end declare section */
#line 138 "db_interface.pgc"

	{ ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_execute, "mark_post_as_deleted_stmt", 
	ECPGt_int,&(post_id),(long)1,(long)1,sizeof(int), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, ECPGt_EOIT, ECPGt_EORT);
#line 139 "db_interface.pgc"

if (sqlca.sqlcode < 0) print_sqlca ( );}
#line 139 "db_interface.pgc"

	{ ECPGtrans(__LINE__, NULL, "commit");
#line 140 "db_interface.pgc"

if (sqlca.sqlcode < 0) print_sqlca ( );}
#line 140 "db_interface.pgc"

}

struct db_thread_array* db_retrieve_threads() {
	static struct db_thread_array thread_list;
	initThreadArray(&thread_list, 25);
	/* exec sql begin declare section */
	 
	 
	
#line 147 "db_interface.pgc"
 int thread_id ;
 
#line 148 "db_interface.pgc"
 int number_of_posts ;
/* exec sql end declare section */
#line 149 "db_interface.pgc"

	/* declare thread_getter cursor for select id , number_of_posts from thread order by id */
#line 152 "db_interface.pgc"

	{ ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_normal, "declare thread_getter cursor for select id , number_of_posts from thread order by id", ECPGt_EOIT, ECPGt_EORT);
#line 153 "db_interface.pgc"

if (sqlca.sqlcode < 0) print_sqlca ( );}
#line 153 "db_interface.pgc"

	/* exec sql whenever not found  break ; */
#line 154 "db_interface.pgc"
 // Exits the while loop when `thread_getter` has retreived all threads
	while (true) {
		{ ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_normal, "fetch thread_getter", ECPGt_EOIT, 
	ECPGt_int,&(thread_id),(long)1,(long)1,sizeof(int), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, 
	ECPGt_int,&(number_of_posts),(long)1,(long)1,sizeof(int), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, ECPGt_EORT);
#line 156 "db_interface.pgc"

if (sqlca.sqlcode == ECPG_NOT_FOUND) break;
#line 156 "db_interface.pgc"

if (sqlca.sqlcode < 0) print_sqlca ( );}
#line 156 "db_interface.pgc"

		struct db_thread_struct thread;
		thread.id = thread_id;
		thread.number_of_posts = number_of_posts;
		insertToThreadArray(&thread_list, thread);
	}
	{ ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_normal, "close thread_getter", ECPGt_EOIT, ECPGt_EORT);
#line 162 "db_interface.pgc"

if (sqlca.sqlcode < 0) print_sqlca ( );}
#line 162 "db_interface.pgc"

	{ ECPGtrans(__LINE__, NULL, "commit");
#line 163 "db_interface.pgc"

if (sqlca.sqlcode < 0) print_sqlca ( );}
#line 163 "db_interface.pgc"

	return &thread_list;
}

// Runs on start-up. Reads every message from the database and inserts it into a list of db_post_struct.
struct db_post_array* db_retrieve_history() {
	static struct db_post_array post_history;
	initPostArray(&post_history, 25);
	/* exec sql begin declare section */
	 
	 
	 
	  
	 
	 
	 
	   
	 
	 
	
#line 172 "db_interface.pgc"
 int post_id ;
 
#line 173 "db_interface.pgc"
 int thread_id ;
 
#line 174 "db_interface.pgc"
 int id_in_thread ;
 
#line 175 "db_interface.pgc"
 unsigned int upload_epoch ;
 
#line 176 "db_interface.pgc"
  struct varchar_1  { int len; char arr[ POST_MAX_NAME + 1 ]; }  post_name ;
 
#line 177 "db_interface.pgc"
 timestamp upload_timestamp ;
 
#line 178 "db_interface.pgc"
  struct varchar_2  { int len; char arr[ POST_MAX_CONTENT + 1 ]; }  post_content ;
 
#line 179 "db_interface.pgc"
 char files [ 4 ] [ POST_MAX_FILE_NAME + 1 ] ;
 
#line 179 "db_interface.pgc"
 int file_indicators [ 4 ] ;
 
#line 180 "db_interface.pgc"
 char deleted ;
 
#line 181 "db_interface.pgc"
 char key [ KEY_LENGTH + 1 ] ;
/* exec sql end declare section */
#line 182 "db_interface.pgc"

	/* declare post_getter cursor for select id , thread , id_in_thread , name , upload_timestamp , content , files [ 0 ] , files [ 1 ] , files [ 2 ] , files [ 3 ] , extract ( epoch from upload_timestamp ) :: int as upload_timestamp_int , deleted , key from post order by id */
#line 185 "db_interface.pgc"

	{ ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_normal, "declare post_getter cursor for select id , thread , id_in_thread , name , upload_timestamp , content , files [ 0 ] , files [ 1 ] , files [ 2 ] , files [ 3 ] , extract ( epoch from upload_timestamp ) :: int as upload_timestamp_int , deleted , key from post order by id", ECPGt_EOIT, ECPGt_EORT);
#line 186 "db_interface.pgc"

if (sqlca.sqlcode < 0) print_sqlca ( );}
#line 186 "db_interface.pgc"

	/* exec sql whenever not found  break ; */
#line 187 "db_interface.pgc"
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
#line 189 "db_interface.pgc"

if (sqlca.sqlcode == ECPG_NOT_FOUND) break;
#line 189 "db_interface.pgc"

if (sqlca.sqlcode < 0) print_sqlca ( );}
#line 189 "db_interface.pgc"

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
#line 211 "db_interface.pgc"

if (sqlca.sqlcode < 0) print_sqlca ( );}
#line 211 "db_interface.pgc"

	{ ECPGtrans(__LINE__, NULL, "commit");
#line 212 "db_interface.pgc"

if (sqlca.sqlcode < 0) print_sqlca ( );}
#line 212 "db_interface.pgc"

	return (&post_history);
}

void db_disconnect() {
	{ ECPGdisconnect(__LINE__, "ALL");
#line 217 "db_interface.pgc"

if (sqlca.sqlcode < 0) print_sqlca ( );}
#line 217 "db_interface.pgc"

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
