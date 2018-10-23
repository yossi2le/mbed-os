/* mbed Microcontroller Library
 * Copyright (c) 2018 ARM Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "FileSystemStore.h"
#include "Dir.h"
#include "File.h"
#include "BlockDevice.h"

#include "mbed_trace.h"
#define TRACE_GROUP "FSST"

using namespace mbed;
//namespace mbed {
/** Enum FSSTF standard error codes
 *
 *  @enum FSSTF_bd_error
 */
 
// enum FSSTF_bd_error {
//     FSST_ERROR_OK                    = 0,     /*!< no error */
// };


// Important data structures
// Key metadata
// typedef struct {
//     uint32_t magic;
//     uint16_t metadata_size;
//     uint16_t revision;
//     uint32_t user_flags;
//     uint32_t data_size;
// } key_metadata_t;

// incremental set handle
// typedef struct {
//     char *key;
//     uint32_t create_flags;
// } inc_set_handle_t;

// iterator handle
// typedef struct {
//     void *dir_handle;
//     char *prefix;
// } key_iterator_handle_t;

/** FileSystemStore for Secure Store
 *
 *  @code
 *  ...
 *  @endcode
 */
FileSystemStore::FileSystemStore(size_t max_keys, FileSystem *fs) : _fs(fs), _max_keys(max_keys), _num_keys(0), _is_initialized(false)
{
	memset(_cfg_fs_path, 0, FSST_PATH_NAME_SIZE+1);
	strncpy(_cfg_fs_path,"$fsst$", FSST_PATH_NAME_SIZE);
	_cfg_fs_path[FSST_PATH_NAME_SIZE] = '\0';
	memset(_full_path_key, 0, FSST_PATH_NAME_SIZE+KVStore::MAX_KEY_LENGTH+1);
	strncpy(_full_path_key, _cfg_fs_path, FSST_PATH_NAME_SIZE);
	strcat(_full_path_key,"/");
}
    	 
// Initialization and reset
int FileSystemStore::init()
{
  int status = FSST_ERROR_OK;

  printf("FileSystemStore Init Enter\n");

  _mutex.lock();

  Dir kv_dir;

  if (kv_dir.open(_fs, _cfg_fs_path) != 0) {
    tr_warning("KV Dir: %s, doesnt exist - creating new.. ", _cfg_fs_path); //TBD verify ERRNO NOEXIST
    _num_keys = 0;
    if (_fs->mkdir(_cfg_fs_path,/* which flags ? */0777) != 0) {
    	tr_error("KV Dir: %s, mkdir failed.. ", _cfg_fs_path); //TBD verify ERRNO NOEXIST
    	status = FSST_ERROR_FS_OPERATION_FAILED;
    	goto exit_point;
    }
  }
  else {
	  tr_warning("KV Dir: %s, exists(verified) - now closing it", _cfg_fs_path);
	  _num_keys = kv_dir.size();
	  if (kv_dir.close() != 0) {
		  tr_error("KV Dir: %s, dir_close failed", _cfg_fs_path); //TBD verify ERRNO NOEXIST
	  }
  }
  
  _is_initialized = true;
exit_point:
  _mutex.unlock();

  return status;

}

int FileSystemStore::_build_full_path_key(const char *key_src)
{
	strncpy(&_full_path_key[strlen(_cfg_fs_path)+1/* for path's \ */], key_src, KVStore::MAX_KEY_LENGTH);
	_full_path_key[(FSST_PATH_NAME_SIZE+KVStore::MAX_KEY_LENGTH)] = '\0';
	return 0;
}


int FileSystemStore::_strip_full_path_from_key(char **stripped_key_ptr_dst, char *full_path_key_src)
{
	if (strstr(full_path_key_src,_cfg_fs_path) != full_path_key_src) {
		return -1;
	}

	*stripped_key_ptr_dst = full_path_key_src + strlen(_cfg_fs_path);
	return 0;
}

int FileSystemStore::deinit()
{
	// Required ??
	//if (false == _is_initiailized) {
	//		return FSST_ERROR_OK
	//	}
	_mutex.lock();
    _is_initialized = false;
	_mutex.unlock();
	return FSST_ERROR_OK;

}

int FileSystemStore::reset()
{
	int status = FSST_ERROR_OK;
	Dir kv_dir;
	struct dirent dir_ent;

	_mutex.lock();
	if (false == _is_initialized) {
		status = FSST_ERROR_NOT_INITIALIZED;
		goto exit_point;
	}

	kv_dir.open(_fs, _cfg_fs_path);

	while (kv_dir.read(&dir_ent) != 0) {
		tr_warning("Looping FSST folder: %s, File - %s", _cfg_fs_path, dir_ent.d_name);
		if (dir_ent.d_type != DT_REG) {
			tr_error("KV_Dir should contain only Regular File - %s", dir_ent.d_name);
			continue;
		}
		// Build File's full path name and delete it
		_build_full_path_key(dir_ent.d_name);
		_fs->remove(/*dir_ent.d_name*/_full_path_key);
	}

	_num_keys = 0;

	/*
	// Delete empty folder
    status = _fs->remove(_cfg_fs_path);
    if (status == 0) {
    	_num_keys = 0;
    }
    else {
    	tr_error("Failed to remove empty KV Dir: %s", _cfg_fs_path);
    }*/
    kv_dir.close();

exit_point:
	_mutex.unlock();
	return status;
}


// Core API
int FileSystemStore::set(const char *key, const void *buffer, size_t size, uint32_t create_flags)
{
	int status = FSST_ERROR_OK;
	set_handle_t handle;

	if (false == _is_initialized) {
		status = FSST_ERROR_NOT_INITIALIZED;
		goto exit_point;
	}

	if ((key == NULL) || (size > KVStore::MAX_KEY_LENGTH)|| ((buffer == NULL) && (size > 0)) ) {
		status = FSST_ERROR_INVALID_INPUT;
		goto exit_point;
	}

	status = set_start(&handle, key, size, create_flags);
	if (status != FSST_ERROR_OK) {
		tr_error("FSST Set set_start Failed: %d", status);
		goto exit_point;
	}

	status = set_add_data(handle, buffer, size);
	if (status != FSST_ERROR_OK) {
		tr_error("FSST Set set_add_data Failed: %d", status);
		goto exit_point;
	}

	status = set_finalize(handle);
	if (status != FSST_ERROR_OK) {
		tr_error("FSST Set set_finalize Failed: %d", status);
		goto exit_point;
	}

exit_point:

	return status;
}

int FileSystemStore::_verify_key_file(const char *key, key_metadata_t *key_metadata, File *kv_file)
{
	int status = FSST_ERROR_OK;
	uint32_t file_size = 0;

	if (key == NULL) {
		status = FSST_ERROR_INVALID_INPUT;
		goto exit_point;
	}

	_build_full_path_key(key);

	if (0 != kv_file->open(_fs, /*key*/ _full_path_key, O_RDONLY) ) {
	  tr_error("Couldn't read: %s", _full_path_key);
	  status = FSST_ERROR_NOT_FOUND;
	  // TBD handle error scenario..?
	  goto exit_point;
	}

	file_size = (uint32_t)kv_file->size();

    //Read Metadata
	kv_file->read(key_metadata, sizeof(key_metadata_t));

	if ((key_metadata->magic != FSST_MAGIC) ||
    	((file_size - key_metadata->metadata_size) != key_metadata->data_size) ||
    	(key_metadata->revision > FSST_REVISION) ) {
    	status = FSST_ERROR_CORRUPTED_DATA;
    	goto exit_point;
    }

exit_point:
    return status;
}

int FileSystemStore::get(const char *key, void *buffer, size_t buffer_size, size_t *actual_size, size_t offset)
{
	int status = FSST_ERROR_OK;

	File kv_file;
	size_t value_actual_size = 0;

	_mutex.lock();
	if (false == _is_initialized) {
		status = FSST_ERROR_NOT_INITIALIZED;
		goto exit_point;
	}

	key_metadata_t key_metadata;

	if ( (status = _verify_key_file(key, &key_metadata, &kv_file)) != FSST_ERROR_OK ) {
		tr_error("File Verification Failed: %s, status: %d", key, status);
		goto exit_point;
	}


	// Actual size is the minimum of buffer_size and remainder of data in file (data_size-offset)
	value_actual_size = buffer_size;
	if (offset > key_metadata.data_size) {
		status = FSST_ERROR_CORRUPTED_DATA;
		goto exit_point;
	}
	else if ((key_metadata.data_size-offset) < buffer_size) {
		value_actual_size = key_metadata.data_size - offset;
	}

	if ((buffer == NULL) && (value_actual_size > 0)){
		status = FSST_ERROR_CORRUPTED_DATA;
		goto exit_point;
	}

	if (actual_size != NULL) {
		*actual_size = value_actual_size;
	}

	kv_file.seek(key_metadata.metadata_size + offset, SEEK_SET);
	// Read remainder of data
	kv_file.read(buffer, value_actual_size);

exit_point:
	if ( (status == FSST_ERROR_OK) ||
	   (status == FSST_ERROR_CORRUPTED_DATA) ){
		kv_file.close();
	}
	_mutex.unlock();

	return status;
}

int FileSystemStore::get_info(const char *key, info_t *info)
{
	int status = FSST_ERROR_OK;

	if (info == NULL) {
		return FSST_ERROR_INVALID_INPUT;
	}

	File kv_file;

	_mutex.lock();

	if (false == _is_initialized) {
		status = FSST_ERROR_NOT_INITIALIZED;
		goto exit_point;
	}

	key_metadata_t key_metadata;

	if ( (status = _verify_key_file(key, &key_metadata, &kv_file)) != FSST_ERROR_OK ) {
		tr_error("File Verification Failed: %s, status: %d", key, status);
		goto exit_point;
	}

    info->size = key_metadata.data_size;
    info->flags = key_metadata.user_flags;

exit_point:
	if (status == FSST_ERROR_OK) {
		kv_file.close();
	}
	_mutex.unlock();

	return status;
}

int FileSystemStore::remove(const char *key)
{

	if (key == NULL) {
		return FSST_ERROR_INVALID_INPUT;
	}

	_mutex.lock();

	int status = FSST_ERROR_OK;

	_build_full_path_key(key);
	if (false == _is_initialized) {
		status = FSST_ERROR_NOT_INITIALIZED;
		goto exit_point;
	}

    status = _fs->remove(/*key */_full_path_key);
/* For NUM Keys support only */
//    if (status == 0) {
//    	_num_keys--;
//    }

exit_point:
	_mutex.unlock();
	return status;
}

// Incremental set API
int FileSystemStore::set_start(set_handle_t *handle, const char *key, size_t final_data_size, uint32_t create_flags)
{
	int status = FSST_ERROR_OK;
	inc_set_handle_t *set_handle = NULL;
	File kv_file;
    key_metadata_t key_metadata;

	_mutex.lock();

	_build_full_path_key(key);

/* For Max Keys use */
//	if (0 !=kv_file.open(_fs, full_path_key, O_RDONLY)) {
//		tr_warning("set_start open(verify exist): %s, does not exist", full_path_key);
//		if (_num_keys == _max_keys) {
//			return FSST_ERROR_MAX_KEYS_REACHED;
//		}
//		_num_keys++;
//	}
//	else {
//		tr_warning("set_start open(verify exist): %s, exists - now closing", full_path_key);
//		kv_file.close();
//	}


	if ( (status = kv_file.open(_fs, _full_path_key, O_WRONLY|O_CREAT|O_TRUNC)) != FSST_ERROR_OK ){
		tr_warning("set_start failed to open: %s, for writing, err: %d", _full_path_key, status);
		status = FSST_ERROR_FS_OPERATION_FAILED;
		goto exit_point;
	}

	set_handle = new inc_set_handle_t;
	set_handle->create_flags = create_flags;
	set_handle->data_size = final_data_size;
	set_handle->key = new char[strnlen(key, KVStore::MAX_KEY_LENGTH)];
    strncpy(set_handle->key, key, KVStore::MAX_KEY_LENGTH);
    set_handle->key[KVStore::MAX_KEY_LENGTH-1] = '\0';

    *handle = (set_handle_t)set_handle;

    key_metadata.magic = FSST_MAGIC;
    key_metadata.metadata_size = sizeof(key_metadata_t);
    key_metadata.revision = FSST_REVISION;
    key_metadata.user_flags = create_flags;
    key_metadata.data_size = final_data_size;

    kv_file.write(&key_metadata, sizeof(key_metadata_t));

    kv_file.close();
exit_point:
    if (status != FSST_ERROR_OK) {
    	_mutex.unlock();
    }
    return status;
}

int FileSystemStore::set_add_data(set_handle_t handle, const void *value_data, size_t data_size)
{
	inc_set_handle_t *set_handle = (inc_set_handle_t *)handle;
	_build_full_path_key(set_handle->key);
	int status = FSST_ERROR_OK;

	File kv_file;
	if (0 != kv_file.open(_fs, /*set_handle->key*/ _full_path_key, O_RDONLY)) {
		status = FSST_ERROR_NOT_FOUND;
		goto exit_point;
	}
	kv_file.close();

	if (0 != kv_file.open(_fs, /*set_handle->key*/ _full_path_key, O_WRONLY|O_APPEND)) {
		status = FSST_ERROR_FS_OPERATION_FAILED;
		goto exit_point;
	}

	kv_file.write(value_data, data_size);

	kv_file.close();

exit_point:
    if (status != FSST_ERROR_OK) {
    	_mutex.unlock();
    }
    return FSST_ERROR_OK;

}

int FileSystemStore::set_finalize(set_handle_t handle)
{
	int status = FSST_ERROR_OK;
	inc_set_handle_t *set_handle = (inc_set_handle_t *)handle;

	if (set_handle == NULL) {
		status =  FSST_ERROR_INVALID_INPUT;
		goto exit_point;
	}

	if (set_handle->key == NULL) {
		status = FSST_ERROR_INVALID_INPUT;
	}
	else {
		delete set_handle->key;
	}

	delete set_handle;

exit_point:
	if (status != FSST_ERROR_INVALID_INPUT) {
		_mutex.unlock();
	}

	return status;
}

// Key iterator
int FileSystemStore::iterator_open(iterator_t *it, const char *prefix)
{
    int status = FSST_ERROR_OK;
    Dir *kv_dir = NULL;
    key_iterator_handle_t *key_it = NULL;

    if (it == NULL) {
		return FSST_ERROR_INVALID_INPUT;
	}

	_mutex.lock();
	if (false == _is_initialized) {
		status = FSST_ERROR_NOT_INITIALIZED;
		goto exit_point;
	}
    key_it = new key_iterator_handle_t;
    key_it->dir_handle = NULL;
    key_it->prefix = NULL;
    if (prefix != NULL) {
    	key_it->prefix = new char[KVStore::MAX_KEY_LENGTH+1];
    	strncpy(key_it->prefix, prefix, KVStore::MAX_KEY_LENGTH);
    	key_it->prefix[KVStore::MAX_KEY_LENGTH-1] = '\0';
    }

    kv_dir = new Dir;
    if (kv_dir->open(_fs, _cfg_fs_path) != 0) {
        tr_error("KV Dir: %s, doesnt exist", _cfg_fs_path); //TBD verify ERRNO NOEXIST
        status = FSST_ERROR_NOT_FOUND;
        goto exit_point;
    }

    key_it->dir_handle = kv_dir;

    *it = (iterator_t)key_it;

exit_point:
	_mutex.unlock();

	return status;
}

int FileSystemStore::iterator_next(iterator_t it, char *key, size_t key_size)
{
	Dir *kv_dir;
	struct dirent kv_dir_ent;
	int status = FSST_ERROR_NOT_FOUND;
	key_iterator_handle_t *key_it = NULL;
	size_t key_name_size = KVStore::MAX_KEY_LENGTH;
	if (key_size < key_name_size) {
		key_name_size = key_size;
	}

	_mutex.lock();
	if (false == _is_initialized) {
		status = FSST_ERROR_NOT_INITIALIZED;
		goto exit_point;
	}

	key_it = (key_iterator_handle_t *)it;
	kv_dir = (Dir *)key_it->dir_handle;

	while (kv_dir->read(&kv_dir_ent) != 0 ) {
		if (kv_dir_ent.d_type != DT_REG) {
			tr_error("KV_Dir should contain only Regular File - %s", kv_dir_ent.d_name);
			continue;
		}

		//_build_full_path_key(full_path_key, kv_dir_ent.d_name); //Is it required or name is already full path?

		//if (_strip_full_path_from_key(kv_dir_ent.d_name, &key_ptr) != 0) {
//			tr_error("Found filed in KV Dir with ");
	//	}

		/* original size parameter was 0 meaning dont care about name just whether file exists in dir*/
		if (key_name_size == 0) {
			status = FSST_ERROR_OK;
			break;
		}
		else if ( (key_it->prefix == NULL) ||
			 (strncmp(kv_dir_ent.d_name, key_it->prefix, strnlen(key_it->prefix, key_name_size-1)) == 0) ) {
			printf("File Name: %s, name size: %d\n", kv_dir_ent.d_name, key_name_size-1);
			strncpy(key, kv_dir_ent.d_name, key_name_size);
			key[key_name_size-1] = '\0';
			status = FSST_ERROR_OK;
			break;
		}
	}

exit_point:
	_mutex.unlock();
	return status;

}

int FileSystemStore::iterator_close(iterator_t it)
{
	int status = FSST_ERROR_OK;
	key_iterator_handle_t *key_it = (key_iterator_handle_t *)it;

	_mutex.lock();
	if (key_it == NULL) {
		status = FSST_ERROR_INVALID_INPUT;
		goto exit_point;
	}

	if (key_it->prefix != NULL) {
		delete key_it->prefix;
	}

	((Dir *)(key_it->dir_handle))->close();

	if (key_it->dir_handle != NULL) {
		delete ((Dir *)(key_it->dir_handle));
	}
	delete key_it;

exit_point:
	_mutex.unlock();
	return status;
}

//} //namespace mbed

