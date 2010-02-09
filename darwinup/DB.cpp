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
							  1,
							  this->m_archives_table->column(0), // serial
							  serial); // convert from bool to int	
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

uint64_t DarwinupDatabase::get_file_serial_from_archive(Archive* archive, const char* path) {
	uint64_t serial = 0;
	this->get_value("file_serial__archive_path",
					(void**)&serial,
					this->m_files_table,
					this->m_files_table->column(0), // serial
					2,                              // number of where conditions
					this->m_files_table->column(1), // archive
					(uint64_t)archive->serial(),
					this->m_files_table->column(8), // path
					path);
	return serial;
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
	uint64_t* c = (uint64_t*)malloc(sizeof(uint64_t));
	if (!c) fprintf(stderr, "Error: ran out of memory in DarwinupDatabase::count_files().\n");
	res = this->count("count_files",
					  (void**)c,
					  this->m_files_table,
					  2, // number of where conditions
					  this->m_files_table->column(1), // archive
					  (uint64_t)archive->serial(),
					  this->m_files_table->column(8), // path
					  path);
	return *c;
}


int DarwinupDatabase::delete_archive(Archive* archive) {
	return this->del(this->m_archives_table, archive->serial());
}

int DarwinupDatabase::delete_archive(uint64_t serial) {
	return this->del(this->m_archives_table, serial) ;
}

int DarwinupDatabase::delete_file(File* file) {
	return this->del(this->m_files_table, file->serial());
}

int DarwinupDatabase::delete_file(uint64_t serial) {
	return this->del(this->m_files_table, serial);
}

int DarwinupDatabase::delete_files(Archive* archive) {
	return this->del("delete_files__archive",
					 this->m_files_table,
					 1,
					 this->m_files_table->column(1),  // archive
					 (uint64_t)archive->serial());
}


