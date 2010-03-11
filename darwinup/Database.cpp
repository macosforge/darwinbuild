/*
 * Copyright (c) 2010 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_BSD_LICENSE_HEADER_START@
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * @APPLE_BSD_LICENSE_HEADER_END@
 */

#include "Database.h"

/**
 * sqlite3_trace callback for debugging
 */
void dbtrace(void* context, const char* sql) {
	fprintf(stderr, "[SQL] %s\n", sql);
}

Database::Database() {
	m_schema_version = 0;
	m_table_max = 2;
	m_table_count = 0;
	m_tables = (Table**)malloc(sizeof(Table*) * m_table_max);
	this->init_cache();
	m_db = NULL;	
	m_path = NULL;
	m_error_size = ERROR_BUF_SIZE;
	m_error = (char*)malloc(m_error_size);
}

Database::Database(const char* path) {
	m_schema_version = 0;
	m_table_max = 2;
	m_table_count = 0;
	m_tables = (Table**)malloc(sizeof(Table*) * m_table_max);
	this->init_cache();
	m_db = NULL;		
	m_path = strdup(path);
	if (!m_path) {
		fprintf(stderr, "Error: ran out of memory when constructing "
				        "database object.\n");
	}
	m_error_size = ERROR_BUF_SIZE;
	m_error = (char*)malloc(m_error_size);
}

Database::~Database() {
	for (uint32_t i = 0; i < m_table_count; i++) {
		delete m_tables[i];
	}
	this->destroy_cache();
	
	sqlite3_finalize(m_begin_transaction);
	sqlite3_finalize(m_rollback_transaction);
	sqlite3_finalize(m_commit_transaction);

	free(m_tables);
	free(m_path);
	free(m_error);
}

uint32_t Database::schema_version() {
	return this->m_schema_version;
}

void Database::schema_version(uint32_t v) {
	this->m_schema_version = v;
}

int Database::init_schema() {
	// do nothing... children should implement this
	return DB_OK;
}

int Database::post_table_creation() {
	// clients can implement this
	return DB_OK;
}

const char* Database::path() {
	return m_path;
}

const char* Database::error() {
	return m_error;
}

int Database::connect() {
	int res = DB_OK;
	
	if (!m_path) {
		fprintf(stderr, "Error: need to specify a path to Database.\n");
		return -1;
	}

	res = this->pre_connect();
	if (res) {
		fprintf(stderr, "Error: pre-connection failed.\n");
		return res;
	}
	
	int exists = is_regular_file(m_path);
	res = sqlite3_open(m_path, &m_db);
	if (res) {
		sqlite3_close(m_db);
		m_db = NULL;
		fprintf(stderr, "Error: unable to connect to database at: %s \n", 
				m_path);
		return res;
	}
	
	res = this->post_connect();
	if (res) {
		fprintf(stderr, "Error: post-connection failed.\n");
		return res;
	}
	
	if (!exists) {
		// create schema since it is empty
		assert(this->create_tables() == 0);
		assert(this->set_schema_version(this->m_schema_version) == 0);
	} else {
		// test schema versions
		uint32_t version = 0;
		if (this->has_information_table()) {
			version = this->get_schema_version();
		}

		if (version < this->m_schema_version) {
			IF_DEBUG("Upgrading schema from %u to %u \n", version, this->m_schema_version);
			assert(this->upgrade_schema(version) == 0);
			assert(this->set_schema_version(this->m_schema_version) == 0);
		}
		if (version > this->m_schema_version) {
			fprintf(stderr, "Error: this client is too old!\n");
			return DB_ERROR;
		}
	}
	
	return res;	
}

int Database::pre_connect() {	
	int res = DB_OK;
	res = this->init_internal_schema();
	res = this->init_schema();
	return res;
}

int Database::post_connect() {
	int res = DB_OK;
	
	// prepare transaction statements
	if (res == DB_OK) 
		res = sqlite3_prepare_v2(m_db, "BEGIN TRANSACTION", 18,
								 &m_begin_transaction, NULL);
	if (res == DB_OK) 
		res = sqlite3_prepare_v2(m_db, "ROLLBACK TRANSACTION", 21,
								 &m_rollback_transaction, NULL);
	if (res == DB_OK) 
		res = sqlite3_prepare_v2(m_db, "COMMIT TRANSACTION", 19,
								 &m_commit_transaction, NULL);	

	// debug settings
	extern uint32_t verbosity;
	if (verbosity & VERBOSE_SQL) {
		sqlite3_trace(m_db, dbtrace, NULL);
	}
		
	return res;
}

int Database::connect(const char* path) {
	this->m_path = strdup(path);
	if (!m_path) {
		fprintf(stderr, "Error: ran out of memory when trying to connect to "
				        "database.\n");
		return 1;
	}
	return this->connect();
}

int Database::begin_transaction() {
	return this->execute(m_begin_transaction);
}

int Database::rollback_transaction() {
	return this->execute(m_rollback_transaction);
}

int Database::commit_transaction() {
	return this->execute(m_commit_transaction);
}

int Database::bind_all_columns(sqlite3_stmt* stmt, Table* table, va_list args) {
	int res = DB_OK;
	int param = 1;
	for (uint32_t i=0; i<table->column_count(); i++) {
		Column* col = table->column(i);
		if (col->is_pk()) continue;
		uint8_t* bdata = NULL;
		uint32_t bsize = 0;
		switch(col->type()) {
			case TYPE_INTEGER:
				res = sqlite3_bind_int64(stmt, param++, va_arg(args, uint64_t));
				break;
			case TYPE_TEXT:
				res = sqlite3_bind_text(stmt, param++, va_arg(args, char*), 
										-1, SQLITE_STATIC);
				break;
			case TYPE_BLOB:
				bdata = va_arg(args, uint8_t*);
				bsize = va_arg(args, uint32_t);
                res = sqlite3_bind_blob(stmt, param++,
										bdata,
										bsize,
										SQLITE_STATIC);
				break;
		}
		if (res != SQLITE_OK) {
			fprintf(stderr, "Error: failed to bind parameter #%d with column #%d"
					        "of type %d table %s \n",
					param, i, col->type(), table->name());
			return res;
		}
	}
	return res;
}

int Database::bind_va_columns(sqlite3_stmt* stmt, uint32_t count, va_list args) {
	return this->bind_columns(stmt, count, 1, args);
}

int Database::bind_columns(sqlite3_stmt* stmt, uint32_t count, int param, 
						   va_list args) {	
	int res = DB_OK;
    for (uint32_t i=0; i<count; i++) {
        Column* col = va_arg(args, Column*);
        va_arg(args, int);
        uint8_t* bdata = NULL;
        uint32_t bsize = 0;
        char* tval;
        switch(col->type()) {
            case TYPE_INTEGER:
                res = sqlite3_bind_int64(stmt, param++, va_arg(args, uint64_t));
                break;
            case TYPE_TEXT:
                tval = va_arg(args, char*);
                res = sqlite3_bind_text(stmt, param++, tval, -1, SQLITE_STATIC);
                break;
            case TYPE_BLOB:
                bdata = va_arg(args, uint8_t*);
                bsize = va_arg(args, uint32_t);
                res = sqlite3_bind_blob(stmt, param++,
                                        bdata,
										bsize,
                                        SQLITE_STATIC);
                break;
		}
        if (res != SQLITE_OK) {
            fprintf(stderr, "Error: failed to bind parameter #%d with column #%d "
					        "of type %d res %d: %s  \n",
					param-1, i, col->type(), res, 
					sqlite3_errmsg(m_db));
            return res;
        }
    }
	return res;
}

#define __get_stmt(expr) \
	sqlite3_stmt* stmt; \
    sqlite3_stmt** pps; \
	char* key = strdup(name); \
	cache_get_and_retain(m_statement_cache, key, (void**)&pps); \
	if (!pps) { \
		va_list args; \
		va_start(args, count); \
		pps = expr; \
		va_end(args); \
		cache_set_and_retain(m_statement_cache, key, pps, 0); \
	} \
    stmt = *pps; \
	free(key);

int Database::count(const char* name, void** output, Table* table, 
					uint32_t count, ...) {
	va_list args;
	va_start(args, count);
	__get_stmt(table->count(m_db, count, args));
	int res = SQLITE_OK;
	res = this->bind_va_columns(stmt, count, args);
	*output = malloc(sizeof(uint64_t));
	assert(*output);
	res = this->step_once(stmt, *(uint8_t**)output, NULL);
	sqlite3_reset(stmt);
	cache_release_value(m_statement_cache, pps);
	va_end(args);
	return res;
}

int Database::get_value(const char* name, void** output, Table* table, 
						Column* value_column, uint32_t count, ...) {
	va_list args;
	va_start(args, count);
	__get_stmt(table->get_column(m_db, value_column, count, args));
	int res = SQLITE_OK;
	this->bind_va_columns(stmt, count, args);
	uint32_t size = value_column->size();
	*output = malloc(size);
	assert(*output);
	res = this->step_once(stmt, (uint8_t*)*output, NULL);
	sqlite3_reset(stmt);
	cache_release_value(m_statement_cache, pps);
	va_end(args);
	return res;
}

int Database::get_column(const char* name, void** output, uint32_t* result_count,
						 Table* table, Column* column, uint32_t count, ...) {
	va_list args;
	va_start(args, count);
	__get_stmt(table->get_column(m_db, column, count, args));
	int res = SQLITE_OK;
	this->bind_va_columns(stmt, count, args);
	uint32_t size = INITIAL_ROWS * column->size();
	*output = malloc(size);
	res = this->step_all(stmt, output, size, result_count);
	sqlite3_reset(stmt);
	cache_release_value(m_statement_cache, pps);
	va_end(args);
	return res;
}

int Database::get_row(const char* name, uint8_t** output, Table* table, 
					  uint32_t count, ...) {
	va_list args;
	va_start(args, count);
	__get_stmt(table->get_row(m_db, count, args));
	int res = SQLITE_OK;
	this->bind_va_columns(stmt, count, args);
	*output = table->alloc_result();
	res = this->step_once(stmt, *output, NULL);
	sqlite3_reset(stmt);
	cache_release_value(m_statement_cache, pps);
	va_end(args);
	return res;
}

int Database::get_row_ordered(const char* name, uint8_t** output, Table* table, 
							  Column* order_by, int order, uint32_t count, ...) {
	va_list args;
	va_start(args, count);
	__get_stmt(table->get_row_ordered(m_db, order_by, order, count, args));
	int res = SQLITE_OK;
	this->bind_va_columns(stmt, count, args);
	*output = table->alloc_result();
	res = this->step_once(stmt, *output, NULL);
	sqlite3_reset(stmt);
	cache_release_value(m_statement_cache, pps);
	va_end(args);
	return res;
}

int Database::get_all_ordered(const char* name, uint8_t*** output, 
							  uint32_t* result_count, Table* table, 
							  Column* order_by, int order, uint32_t count, ...) {
	va_list args;
	va_start(args, count);
	__get_stmt(table->get_row_ordered(m_db, order_by, order, count, args));
	int res = SQLITE_OK;
	this->bind_va_columns(stmt, count, args);
	uint8_t* current = NULL;
	*result_count = 0;
	uint32_t output_max = INITIAL_ROWS;
	*output = (uint8_t**)calloc(output_max, sizeof(uint8_t*));
	
	res = SQLITE_ROW;
	while (res == SQLITE_ROW) {
		if ((*result_count) >= output_max) {
			output_max *= REALLOC_FACTOR;
			*output = (uint8_t**)realloc((*output), output_max * sizeof(uint8_t*));
			if (!(*output)) {
				fprintf(stderr, "Error: ran out of memory trying to realloc output"
						        "in get_all_ordered.\n");
				return DB_ERROR;
			}
		}
		current = table->alloc_result();
		res = this->step_once(stmt, current, NULL);
		if (res == SQLITE_ROW) {
			(*output)[(*result_count)] = current;
			(*result_count)++;
		} else {
			table->free_result(current);
		}
	}

	sqlite3_reset(stmt);
	cache_release_value(m_statement_cache, pps);
	va_end(args);
	return res;
}

int Database::update_value(const char* name, Table* table, Column* value_column, 
						   void** value, uint32_t count, ...) {
	va_list args;
	va_start(args, count);
	__get_stmt(table->update_value(m_db, value_column, count, args));
	int param = 1;
	int res = SQLITE_OK;
	switch(value_column->type()) {
		case TYPE_INTEGER:
			res = sqlite3_bind_int64(stmt, param++, (uint64_t)*value);
			break;
		case TYPE_TEXT:
			res = sqlite3_bind_text(stmt, param++, (char*)*value, -1, SQLITE_STATIC);
			break;
			// XXX: support blob columns here
		case TYPE_BLOB:
			fprintf(stderr, "Error: Database::update_value() not implemented for "
					        "BLOB columns.\n");
			assert(false);
	}
	if (res != SQLITE_OK) {
		fprintf(stderr, "Error: update_value failed to bind value with value_column "
				        "type %d in table %s. \n",
				value_column->type(), table->name());
		return res;
	}
	this->bind_columns(stmt, count, param, args);
	res = sqlite3_step(stmt);
	sqlite3_reset(stmt);
    cache_release_value(m_statement_cache, pps);
	va_end(args);
	return (res == SQLITE_DONE ? SQLITE_OK : res);
}

int Database::del(const char* name, Table* table, uint32_t count, ...) {
	va_list args;
	va_start(args, count);
	__get_stmt(table->del(m_db, count, args));
	int res = SQLITE_OK;
	this->bind_va_columns(stmt, count, args);
	if (res == SQLITE_OK) res = this->execute(stmt);
	va_end(args);
	return res;
	
}

/**
 * Given a table and an arg list in the same order as Table::add_column() calls,
 * binds and executes a sql update. The Table is responsible for preparing the
 * statement in Table::update()
 *
 * All integer args must be cast to uint64_t
 * All blob columns must provide 2 args in the list. The first arg is a uint8_t* 
 * of data and then the uint32_t value for size of the data. 
 *
 */
int Database::update(Table* table, uint64_t pkvalue, ...) {
	va_list args;
	va_start(args, pkvalue);

	int res = SQLITE_OK;
	
	// get the prepared statement
	sqlite3_stmt* stmt = table->update(m_db);
	if (!stmt) {
		fprintf(stderr, "Error: %s table gave a NULL statement when trying to "
				        "update.\n", table->name());
		return 1;
	}
	
	this->bind_all_columns(stmt, table, args);
	
	// bind the primary key in the WHERE clause
	//  bind_all_columns already bound the first n'th params, where n in the
	//  table's column count, so we provide that count as the parameter value
	if (res==SQLITE_OK) res = sqlite3_bind_int64(stmt, table->column_count(), 
												 pkvalue);
	if (res==SQLITE_OK) res = this->execute(stmt);
	va_end(args);
	return res;
}

int Database::insert(Table* table, ...) {
	va_list args;
	va_start(args, table);

	int res = SQLITE_OK;
	// get the prepared statement
	sqlite3_stmt* stmt = table->insert(m_db);
	if (!stmt) {
		fprintf(stderr, "Error: %s table gave a NULL statement when trying to "
				        "insert.\n", table->name());
		return 1;
	}
	this->bind_all_columns(stmt, table, args);
	if (res == SQLITE_OK) res = this->execute(stmt);
	va_end(args);
	return res;
}

#undef __get_stmt

int Database::del(Table* table, uint64_t serial) {
	int res = SQLITE_OK;
	sqlite3_stmt* stmt = table->del(m_db);
	if (!stmt) {
		fprintf(stderr, "Error: %s table gave a NULL statement when trying to "
				        "delete.\n", table->name());
		return res;
	}
	if (res == SQLITE_OK) res = sqlite3_bind_int64(stmt, 1, serial);
	if (res == SQLITE_OK) res = this->execute(stmt);
	return res;
}

uint64_t Database::last_insert_id() {
	return (uint64_t)sqlite3_last_insert_rowid(m_db);
}



int Database::sql_once(const char* fmt, ...) {
	int res = 0;
    va_list args;
    va_start(args, fmt);
    char* error;
    if (this->m_db) {
        char *query = sqlite3_vmprintf(fmt, args);
        res = sqlite3_exec(this->m_db, query, NULL, NULL, &error);
        sqlite3_free(query);
    } else {
        fprintf(stderr, "Error: database not open.\n");
        res = SQLITE_ERROR;
    }
    va_end(args);
	if (error) {
		strlcpy(m_error, error, m_error_size);
		fprintf(stderr, "Error: sql(): %s \n", m_error);
		fprintf(stderr, "Error: fmt: %s \n", fmt);
		sqlite3_free(error);
	}
	return res;
}

int Database::sql(const char* name, const char* fmt, ...) {
	sqlite3_stmt* stmt;
	char* key = strdup(name);
	cache_get_and_retain(m_statement_cache, key, (void**)&stmt);
	if (!stmt) {
		va_list args;
		va_start(args, fmt);
		char* query = sqlite3_vmprintf(fmt, args);
		int res = sqlite3_prepare_v2(m_db, query, strlen(query), &stmt, NULL);
		va_end(args);
		if (res != SQLITE_OK) {
			fprintf(stderr, "Error: unable to prepare statement for query: %s\n"
					        "Error: %s\n",
					query, sqlite3_errmsg(m_db));
			free(key);
			return res;
		}
		cache_set_and_retain(m_statement_cache, key, stmt, 0); \
		free(key);
	}
	return this->execute(stmt);
}

int Database::execute(sqlite3_stmt* stmt) {
	int res = sqlite3_step(stmt);
	if (res == SQLITE_DONE) {
		res = SQLITE_OK;
	} else {
		strlcpy(m_error, sqlite3_errmsg(m_db), m_error_size);
		fprintf(stderr, "Error: execute() error: %s \n", m_error);
	}
	res = sqlite3_reset(stmt);
	return res;
}

int Database::add_table(Table* t) {
	if (m_table_count >= m_table_max) {
		m_tables = (Table**)realloc(m_tables, 
									m_table_max*sizeof(Table*)*REALLOC_FACTOR);
		if (!m_tables) {
			fprintf(stderr, "Error: unable to reallocate memory to add a "
					"table\n");
			return 1;
		}
		m_table_max *= REALLOC_FACTOR;
	}
	m_tables[m_table_count++] = t;
	t->m_version = this->m_schema_version;
	
	return 0;
}

/**
 * get a row count of the first table to detect if the schema
 * needs to be initialized
 */
bool Database::is_empty() {
	if (!m_tables[0]) {
		fprintf(stderr, "Warning: Database has not had a schema initialized.\n");
		return false;
	}
	int res = SQLITE_OK;
	char* query;
	asprintf(&query, "SELECT count(*) FROM %s;", m_tables[0]->name());
	res = sqlite3_exec(this->m_db, query, NULL, NULL, NULL);
	free(query);
	return res != SQLITE_OK;
}

int Database::create_table(Table* table) {
	int res = this->sql_once(table->create());
	if (res != DB_OK) {
		fprintf(stderr, "Error: sql error trying to create"
				" table: %s: %s\n",
				table->name(), m_error);
	}
	return res;
}

int Database::create_tables() {
	int res = SQLITE_OK;
	for (uint32_t i=0; i<m_table_count; i++) {
		this->sql_once(m_tables[i]->create());
		if (res!=SQLITE_OK) {
			fprintf(stderr, "Error: sql error trying to create table: %s: %s\n",
					m_tables[i]->name(), m_error);
			return res;
		}
	}
	if (res == DB_OK) res = this->initial_data();
	if (res == DB_OK) res = this->post_table_creation();
	return res;
}

int Database::upgrade_schema(uint32_t version) {
	int res = DB_OK;
	this->begin_transaction();
	
	res = this->upgrade_internal_schema(version);
	if (res != DB_OK) {
		fprintf(stderr, "Error: unable to upgrade internal schema.\n");
		this->rollback_transaction();
		return res;
	}			
	
	for (uint32_t ti = 0; res == DB_OK && ti < m_table_count; ti++) {
		if (m_tables[ti]->version() > version) {
			// entire table is new
			res = this->create_table(m_tables[ti]);
		} else {
			// table is same version, so check for new columns
			for (uint32_t ci = 0; res == DB_OK && ci < m_tables[ti]->column_count(); ci++) {
				if (m_tables[ti]->column(ci)->version() < version) {
					// this should never happen
					fprintf(stderr, "Error: internal error with schema versioning."
									" Column %s is older than its table %s. \n",
							m_tables[ti]->column(ci)->name(), m_tables[ti]->name());
				}
				if (m_tables[ti]->column(ci)->version() > version) {
					// column is new
					res = this->sql_once(m_tables[ti]->alter_add_column(ci));
					if (res != DB_OK) {
						fprintf(stderr, "Error: sql error trying to upgrade (alter)"
						        " table: %s column: %s : %s\n",
								m_tables[ti]->name(), m_tables[ti]->column(ci)->name(), 
								m_error);
					}		
				}
			}
		}
	}
	
	if (res == DB_OK) {
		this->commit_transaction();
	} else {
		this->rollback_transaction();
	}

	return res;
}

int Database::upgrade_internal_schema(uint32_t version) {
	int res = DB_OK;
	
	if (version == 0) {
		res = this->sql_once(this->m_information_table->create());
	}
	
	return res;
}

int Database::init_internal_schema() {
	this->m_information_table = new Table("database_information");
	ADD_TABLE(this->m_information_table);
	ADD_PK(m_information_table, "id");
	ADD_INDEX(m_information_table, "variable", TYPE_TEXT, true);
	ADD_TEXT(m_information_table, "value");
	return DB_OK;
}

int Database::initial_data() {
	// load our initial config data
	return this->insert(m_information_table, "schema_version", "0");
}

int Database::get_information_value(const char* variable, char*** value) {
	return this->get_value("get_information_value",
						   (void**)value,
						   this->m_information_table,
						   this->m_information_table->column(2), // value
						   1,
						   this->m_information_table->column(1), // variable
						   '=', variable);
}

int Database::update_information_value(const char* variable, const char* value) {
	int res = SQLITE_OK;
	uint64_t* c;
	res = this->count("count_info_var",
					  (void**)&c,
					  this->m_information_table,
					  1,
					  this->m_information_table->column(1), // variable
					  '=', variable);
					  
	if (*c > 0) {
		res = this->update_value("update_information_value",
								  this->m_information_table,
								  this->m_information_table->column(2), // value
								  (void**)&value, 
								  1,
								  this->m_information_table->column(1), // variable
								  '=', variable);
	} else {
		res = this->insert(m_information_table, variable, value);
	}
	
	return res;
}

bool Database::has_information_table() {
	int res = sqlite3_exec(this->m_db, 
						   "SELECT count(*) FROM database_information;", 
						   NULL, NULL, NULL);
	return res == SQLITE_OK;
}

uint32_t Database::get_schema_version() {
	int res = SQLITE_OK;
	char** vertxt = NULL;
	res = this->get_information_value("schema_version", &vertxt);
	if (res == SQLITE_ROW) {
		uint32_t version = (uint32_t)strtoul(*vertxt, NULL, 10);
		free(*vertxt);
		return version;
	} else {
		// lack of information table/value means we are 
		//  upgrading an old database
		return 0;
	}
}

int Database::set_schema_version(uint32_t version) {
	IF_DEBUG("set_schema_version %u \n", version);
	int res = DB_OK;
	char* vertxt;
	asprintf(&vertxt, "%u", version);
	if (!vertxt) return DB_ERROR;
	res = this->update_information_value("schema_version", vertxt);
	free(vertxt);
	return res;
}

size_t Database::store_column(sqlite3_stmt* stmt, int column, uint8_t* output) {
	size_t used;
	int type = sqlite3_column_type(stmt, column);
	const void* blob;
	int blobsize;
	switch(type) {
		case SQLITE_INTEGER:
			*(uint64_t*)output = (uint64_t)sqlite3_column_int64(stmt, column);
			used = sizeof(uint64_t);
			break;
		case SQLITE_TEXT:
			*(const char**)output = strdup((const char*)sqlite3_column_text(stmt, 
																			column));
			used = sizeof(char*);
			break;
		case SQLITE_BLOB:
			blob = sqlite3_column_blob(stmt, column);
			blobsize = sqlite3_column_bytes(stmt, column);
			*(void**)output = malloc(blobsize);
			if (*(void**)output && blobsize) {
				memcpy(*(void**)output, blob, blobsize);
			} else {
				fprintf(stderr, "Error: unable to get blob from database stmt.\n");
			}
			used = sizeof(void*);
			break;
		case SQLITE_NULL:
			// result row has a NULL value which is okay
			*(const char**)output = NULL;
			used = sizeof(char*);
			break;
		default:
			fprintf(stderr, "Error: unhandled column type in "
							"Database::store_column(): %d \n", 
					type);
			return 0;
	}
	
	return used;
}

/**
 *   will not realloc memory for output since caller should know how
 *   much to alloc in the first place. Sets used to be how many bytes
 *   were written to output
 */
int Database::step_once(sqlite3_stmt* stmt, uint8_t* output, uint32_t* used) {
	int res = sqlite3_step(stmt);
	uint8_t* current = output;
	if (used) *used = 0;
	if (res == SQLITE_ROW) {
		int count = sqlite3_column_count(stmt);
		for (int i = 0; i < count; i++) {
			current += this->store_column(stmt, i, current);
		}
		if (used) {
			*used = current - output;
		}
	}

	return res;
}

int Database::step_all(sqlite3_stmt* stmt, void** output, uint32_t size, 
					   uint32_t* count) {
	uint32_t used = 0;
	uint32_t total_used = used;
	uint32_t rowsize = size / INITIAL_ROWS;
	uint8_t* current = *(uint8_t**)output;
	*count = 0;
	int res = SQLITE_ROW;
	while (res == SQLITE_ROW) {
		current = *(uint8_t**)output + total_used;
		res = this->step_once(stmt, current, &used);
		if (res == SQLITE_ROW) (*count)++;
		total_used += used;
		if (total_used >= (size - rowsize)) {
			size *= REALLOC_FACTOR;
			*output = realloc(*output, size);
			if (!*output) {
				fprintf(stderr, "Error: ran out of memory in Database::step_all \n");
				return SQLITE_ERROR;
			}
		}		
	}
    sqlite3_reset(stmt);
	return res;
}


/**
 *
 *  libcache
 *
 */
void Database::init_cache() {
	cache_attributes_t attrs;
	attrs.version = CACHE_ATTRIBUTES_VERSION_2;
	attrs.key_hash_cb = cache_key_hash_cb_cstring;
	attrs.key_is_equal_cb = cache_key_is_equal_cb_cstring;
	attrs.key_retain_cb = cache_key_retain;
	attrs.key_release_cb = cache_release_cb_free;
	attrs.value_release_cb = cache_statement_release;
	attrs.value_retain_cb = NULL;
	attrs.value_make_purgeable_cb = NULL;
	attrs.value_make_nonpurgeable_cb = NULL;
	attrs.user_data = NULL;
	cache_create("org.macosforge.darwinbuild.darwinup.statements", 
				 &attrs, &m_statement_cache);
}

void Database::destroy_cache() {
	cache_destroy(m_statement_cache);
}

void cache_key_retain(void* key_in, void** key_out, void* user_data) {
	*key_out = strdup((char*)key_in);
}

void cache_statement_release(void* value, void* user_data) {
	sqlite3_finalize(*(sqlite3_stmt**)value);
}
