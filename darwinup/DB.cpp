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

#include "DB.h"


DarwinupDatabase::DarwinupDatabase() {
	this->connect();
}

DarwinupDatabase::DarwinupDatabase(const char* path) : Database(path) {
	this->connect();
}

DarwinupDatabase::~DarwinupDatabase() {
	// parent automatically deallocates schema objects
}

void DarwinupDatabase::init_schema() {
	// XXX: use macros to make this cleaner
	
	this->m_archives_table = new Table("archives");
	//                                                                           index  pk     unique
	assert(m_archives_table->add_column(new Column("serial",     TYPE_INTEGER,   false, true,  false))==0);
	assert(m_archives_table->add_column(new Column("uuid",       TYPE_BLOB,      true,  false, true))==0);
	assert(m_archives_table->add_column(new Column("name",       TYPE_TEXT))==0);
	assert(m_archives_table->add_column(new Column("date_added", TYPE_INTEGER))==0);
	assert(m_archives_table->add_column(new Column("active",     TYPE_INTEGER))==0);
	assert(m_archives_table->add_column(new Column("info",       TYPE_INTEGER))==0);
	assert(this->add_table(this->m_archives_table)==0);
	
	this->m_files_table = new Table("files");
	//                                                                           index  pk     unique
	assert(m_files_table->add_column(new Column("serial",  TYPE_INTEGER,         false, true,  false))==0);
	assert(m_files_table->add_column(new Column("archive", TYPE_INTEGER))==0);
	assert(m_files_table->add_column(new Column("info",    TYPE_INTEGER))==0);
	assert(m_files_table->add_column(new Column("mode",    TYPE_INTEGER))==0);
	assert(m_files_table->add_column(new Column("uid",     TYPE_INTEGER))==0);
	assert(m_files_table->add_column(new Column("gid",     TYPE_INTEGER))==0);
	assert(m_files_table->add_column(new Column("size",    TYPE_INTEGER))==0);
	assert(m_files_table->add_column(new Column("digest",  TYPE_BLOB))==0); 
	assert(m_files_table->add_column(new Column("path",    TYPE_TEXT,            true,  false, false))==0);
	assert(this->add_table(this->m_files_table)==0);	
}

int DarwinupDatabase::activate_archive(uint64_t serial) {
	uint64_t active = 1;
	return this->set_archive_active(serial, &active);
}

int DarwinupDatabase::deactivate_archive(uint64_t serial) {
	uint64_t active = 0;
	return this->set_archive_active(serial, &active);
}

int DarwinupDatabase::set_archive_active(uint64_t serial, uint64_t* active) {
	return this->update_value("activate_archive", 
							  this->m_archives_table,
							  this->m_archives_table->column(4), // active
							  (void**)active,
							  1,                                 // number of where conditions
							  this->m_archives_table->column(0), // serial
							  '=', serial);
}

int DarwinupDatabase::update_archive(uint64_t serial, uuid_t uuid, const char* name,
									  time_t date_added, uint32_t active, uint32_t info) {
	return this->update(this->m_archives_table, serial,
						(uint8_t*)uuid,
						(uint32_t)sizeof(uuid_t),
						name,
						(uint64_t)date_added,
						(uint64_t)active,
						(uint64_t)info);
}

uint64_t DarwinupDatabase::insert_archive(uuid_t uuid, uint32_t info, const char* name, 
										  time_t date_added) {
	
	int res = this->insert(this->m_archives_table,
						   (uint8_t*)uuid,
						   (uint32_t)sizeof(uuid_t),
						   name,
						   (uint64_t)date_added,
						   (uint64_t)0,
						   (uint64_t)info);
	if (res != SQLITE_OK) {
		fprintf(stderr, "Error: unable to insert archive %s: %s \n",
				name, this->error());
		return 0;
	}
	
	return this->last_insert_id();
}

File* DarwinupDatabase::make_file(uint8_t* data) {
	// XXX do this with a for loop and column->type()
	uint64_t serial;
	memcpy(&serial, &data[this->file_offset(0)], sizeof(uint64_t));
	uint64_t archive_serial;
	memcpy(&archive_serial, &data[this->file_offset(1)], sizeof(uint64_t));
	
	IF_DEBUG("make_file offset %d found serial %llu \n", this->file_offset(1), archive_serial);
	
	uint64_t info;
	memcpy(&info, &data[this->file_offset(2)], sizeof(uint64_t));
	uint64_t mode;
	memcpy(&mode, &data[this->file_offset(3)], sizeof(uint64_t));
	uint64_t uid;
	memcpy(&uid, &data[this->file_offset(4)], sizeof(uint64_t));
	uint64_t gid;
	memcpy(&gid, &data[this->file_offset(5)], sizeof(uint64_t));
	uint64_t size;
	memcpy(&size, &data[this->file_offset(6)], sizeof(uint64_t));

	Digest* digest = new Digest();
	digest->m_size = 20; // size of SHA1 hash
	memcpy(digest->m_data, &data[this->file_offset(7)], 20);

	char* path;
	memcpy(&path, &data[this->file_offset(8)], sizeof(char*));
	
	uint8_t* archive_data;
	int res = this->get_archive(&archive_data, archive_serial);
	Archive* archive = NULL;
	if (FOUND(res)) {
		archive = this->make_archive(archive_data);
	} else {
		fprintf(stderr, "Error: DB::make_file could not find the archive for file: %s: %d \n", path, res);
		return NULL;
	}
	this->m_archives_table->free_result(archive_data);
	
	File* result = FileFactory(serial, archive, info, (const char*)path, mode, uid, gid, size, digest);
	this->m_files_table->free_result(data);
	
	return result;
}



int DarwinupDatabase::get_next_file(uint8_t** data, File* file, file_starseded_t star) {
	int res = SQLITE_OK;
	
	char comp = '<';
	const char* name = "file_preceded";
	int order = ORDER_BY_DESC;
	if (star == FILE_SUPERSEDED) {
		comp = '>';
		name = "file_superseded";
		order = ORDER_BY_ASC;
	}
	res = this->get_row_ordered(name,
								data,
								this->m_files_table,
								this->m_files_table->column(1), // order by archive
								order,
								2,
								this->m_files_table->column(1), // archive
								comp, file->archive()->serial(),
								this->m_files_table->column(8), // path
								'=', file->path());
	
	if (res == SQLITE_ROW) return (DB_FOUND | DB_OK);
	if (res == SQLITE_DONE) return DB_OK;
	return DB_ERROR;
}

int DarwinupDatabase::get_file_serial_from_archive(Archive* archive, const char* path, uint64_t** serial) {
	int res = this->get_value("file_serial__archive_path",
							  (void**)serial,
							  this->m_files_table,
							  this->m_files_table->column(0), // serial
							  2,                              // number of where conditions
							  this->m_files_table->column(1), // archive
							  '=', (uint64_t)archive->serial(),
							  this->m_files_table->column(8), // path
							  '=', path);
	
	if (res == SQLITE_ROW) return (DB_FOUND | DB_OK);
	if (res == SQLITE_DONE) return DB_OK;
	return DB_ERROR;
}

int DarwinupDatabase::update_file(uint64_t serial, Archive* archive, uint32_t info, mode_t mode, 
								   uid_t uid, gid_t gid, Digest* digest, const char* path) {

	int res = SQLITE_OK;
								  
	// update the information
	res = this->update(this->m_files_table, serial,
					   (uint64_t)archive->serial(),
					   (uint64_t)info,
					   (uint64_t)mode,
					   (uint64_t)uid,
					   (uint64_t)gid,
					   (uint64_t)0, 
					   (uint8_t*)(digest ? digest->data() : NULL), 
					   (uint32_t)(digest ? digest->size() : 0), 
					   path);

	if (res != SQLITE_OK) {
		fprintf(stderr, "Error: unable to update file with serial %llu and path %s: %s \n",
				serial, path, this->error());
	}
	
	return res;
}
										  
uint64_t DarwinupDatabase::insert_file(uint32_t info, mode_t mode, uid_t uid, gid_t gid, 
									   Digest* digest, Archive* archive, const char* path) {
	
	int res = this->insert(this->m_files_table,
							(uint64_t)archive->serial(),
							(uint64_t)info,
							(uint64_t)mode,
							(uint64_t)uid,
							(uint64_t)gid,
							(uint64_t)0, 
							(uint8_t*)(digest ? digest->data() : NULL), 
							(uint32_t)(digest ? digest->size() : 0), 
							path);
	if (res != SQLITE_OK) {
		fprintf(stderr, "Error: unable to insert file at %s: %s \n",
				path, this->error());
		return 0;
	}
	
	return this->last_insert_id();
}

uint64_t DarwinupDatabase::count_files(Archive* archive, const char* path) {
	int res = SQLITE_OK;
	uint64_t* c;
	res = this->count("count_files",
					  (void**)&c,
					  this->m_files_table,
					  2,                              // number of where conditions
					  this->m_files_table->column(1), // archive
					  '=', (uint64_t)archive->serial(),
					  this->m_files_table->column(8), // path
					  '=', path);	
	if (res != SQLITE_ROW) {
		fprintf(stderr, "Error: unable to count files: %d \n", res);
		return 0;
	}
	return *c;
}

uint64_t DarwinupDatabase::count_archives(bool include_rollbacks) {
	int res = SQLITE_OK;
	uint64_t* c;
	if (include_rollbacks) {
		res = this->count("count_archives",
						  (void**)&c,
						  this->m_archives_table, 0);				
	} else {
		res = this->count("count_archives_norollback",
						  (void**)&c,
						  this->m_archives_table,
						  1,
						  this->m_archives_table->column(2), // name
						  '!', "<Rollback>");		
	}
	if (res != SQLITE_ROW) {
		fprintf(stderr, "Error: unable to count archives: %d \n", res);
		return 0;
	}	
	return *c;	
}

int DarwinupDatabase::delete_archive(Archive* archive) {
	int res = this->del(this->m_archives_table, archive->serial());
	if (res != SQLITE_OK) return DB_ERROR;
	return DB_OK;
}

int DarwinupDatabase::delete_archive(uint64_t serial) {
	int res = this->del(this->m_archives_table, serial);
	if (res != SQLITE_OK) return DB_ERROR;
	return DB_OK;
}

int DarwinupDatabase::delete_empty_archives() {
	int res = this->sql("delete_empty_archives", 
						"DELETE FROM archives "
						"WHERE serial IN "
						" (SELECT serial FROM archives "
						"  WHERE serial NOT IN "
						"   (SELECT DISTINCT archive FROM files));");	
	if (res != SQLITE_OK) return DB_ERROR;
	return DB_OK;
}

int DarwinupDatabase::delete_file(File* file) {
	int res = this->del(this->m_files_table, file->serial());
	if (res != SQLITE_OK) return DB_ERROR;
	return DB_OK;
}

int DarwinupDatabase::delete_file(uint64_t serial) {
	int res = this->del(this->m_files_table, serial);
	if (res != SQLITE_OK) return DB_ERROR;
	return DB_OK;
}

int DarwinupDatabase::delete_files(Archive* archive) {
	int res = this->del("delete_files__archive",
						this->m_files_table,
						1,                               // number of where conditions
						this->m_files_table->column(1),  // archive
						'=', (uint64_t)archive->serial());
	if (res != SQLITE_OK) return DB_ERROR;
	return DB_OK;
}


int DarwinupDatabase::get_inactive_archive_serials(uint64_t** serials, uint32_t* count) {
	int res = this->get_column("inactive_archive_serials",
							   (void**)serials, count,
							   this->m_archives_table,
							   this->m_archives_table->column(0), // serial
							   1,
							   this->m_archives_table->column(4), // active
							   '=', (uint64_t)0);
	if (res == SQLITE_DONE && *count) return (DB_OK & DB_FOUND);
	if (res == SQLITE_DONE) return DB_OK;
	return DB_ERROR;
}

int DarwinupDatabase::get_file_serials(uint64_t** serials, uint32_t* count) {
	int res = this->get_column("file_serials", (void**)serials, count, 
							   this->m_files_table,
							   this->m_files_table->column(0),
							   0);
	if (res == SQLITE_DONE && *count) return (DB_OK & DB_FOUND);
	if (res == SQLITE_DONE) return DB_OK;
	return DB_ERROR;	
}


Archive* DarwinupDatabase::make_archive(uint8_t* data) {
	// XXX do this with a for loop and column->type()
	uint64_t serial;
	memcpy(&serial, &data[this->archive_offset(0)], sizeof(uint64_t));
	uuid_t* uuid;
	memcpy(&uuid, &data[this->archive_offset(1)], sizeof(uuid_t*));
	char* name;
	memcpy(&name, &data[this->archive_offset(2)], sizeof(char*));
	time_t date_added;
	memcpy(&date_added, &data[this->archive_offset(3)], sizeof(time_t));
	uint64_t info;
	memcpy(&info, &data[this->archive_offset(5)], sizeof(uint64_t));

	Archive* archive = new Archive(serial, *uuid, name, NULL, info, date_added);
	this->m_archives_table->free_result(data);
	return archive;
}

int DarwinupDatabase::get_archive(uint8_t** data, uuid_t uuid) {
	int res = this->get_row("archive__uuid",
							data,
							this->m_archives_table,
							1,
							this->m_archives_table->column(1), // uuid
							'=', uuid, sizeof(uuid_t));
	if (res == SQLITE_ROW) return (DB_FOUND | DB_OK);
	if (res == SQLITE_DONE) return DB_OK;
	return DB_ERROR;	
}

int DarwinupDatabase::get_archive(uint8_t** data, uint64_t serial) {
	IF_DEBUG("get_archive serial: %llu \n", serial);
	int res = this->get_row("archive__serial",
							data,
							this->m_archives_table,
							1,
							this->m_archives_table->column(0), // serial
							'=', serial);
	if (res == SQLITE_ROW) return (DB_FOUND | DB_OK);
	if (res == SQLITE_DONE) return DB_OK;
	return DB_ERROR;	
}

int DarwinupDatabase::get_archive(uint8_t** data, const char* name) {
	int res = this->get_row("archive__name",
							data,
							this->m_archives_table,
							1,
							this->m_archives_table->column(2), // name
							'=', name);
	if (res == SQLITE_ROW) return (DB_FOUND | DB_OK);
	if (res == SQLITE_DONE) return DB_OK;
	return DB_ERROR;	
}

int DarwinupDatabase::get_archive(uint8_t** data, archive_keyword_t keyword) {
	int res = SQLITE_OK;
	int order = ORDER_BY_DESC;

	if (keyword == DEPOT_ARCHIVE_OLDEST) {
		order = ORDER_BY_ASC;
	}
	
	res = this->get_row_ordered("archive__keyword",
								data,
								this->m_archives_table,
								this->m_archives_table->column(3), // order by date_added
								order,
								1,
								this->m_archives_table->column(2), // name
								'!', "<Rollback>");
	
	if (res == SQLITE_ROW) return (DB_FOUND | DB_OK);
	if (res == SQLITE_DONE) return DB_OK;
	return DB_ERROR;	
}

int DarwinupDatabase::archive_offset(int column) {
	return this->m_archives_table->offset(column);
}

int DarwinupDatabase::file_offset(int column) {
	return this->m_files_table->offset(column);
}
