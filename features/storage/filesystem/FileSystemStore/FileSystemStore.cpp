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
#include "BlockDevice.h"


//using namespace mbed;
namespace mbed {
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
	memset(_cfg_fs_path, 0, FSST_PATH_NAME_SIZE);
	strncpy(_cfg_fs_path,"$fsst$", FSST_PATH_NAME_SIZE);
	_cfg_fs_path[FSST_PATH_NAME_SIZE] = '\0';
}
    	 
// Initialization and reset
int FileSystemStore::init()
{
  int status = FSST_ERROR_OK;
  fs_dir_t kv_dir;


  _mutex.lock();

  if (_fs->dir_open(&kv_dir, _cfg_fs_path) != 0) {
    tr_info("KV Dir: %s, doesnt exist - creating new.. ", _cfg_fs_path); //TBD verify ERRNO NOEXIST
    _num_keys = 0;
    if (_fs->mkdir(_cfg_fs_path,/* which flags ? */0777) != 0) {
    	tr_error("KV Dir: %s, mkdir failed.. ", _cfg_fs_path); //TBD verify ERRNO NOEXIST
    	status = FSST_ERROR_FS_OPERATION_FAILED;
    	goto exit_point;
    }
  }
  else {
	  _num_keys = _fs->dir_size();
	  if (_fs->dir_close(kv_dir) != 0) {
		  tr_error("KV Dir: %s, dir_close failed", _cfg_fs_path); //TBD verify ERRNO NOEXIST
	  }
  }

  
  _is_initialized = true;
exit_point:
  _mutex.unlock();

  return FSST_ERROR_OK;

}

int FileSystemStore::_build_full_path_key(char *full_path_key_dst, const char *key_src)
{
	strncpy(full_path_key_dst,_cfg_fs_path, FSST_PATH_NAME_SIZE);
	strncat(full_path_key_dst, key_src, FSST_MAX_KEY_LEN);
	full_path_key_dst[(FSST_PATH_NAME_SIZE+FSST_MAX_KEY_LEN)] = '\0';
	return 0;
}


int FileSystemStore::_strip_full_path_from_key(char **stripped_key_ptr_dst, const char *full_path_key_src)
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

}

int FileSystemStore::reset()
{
	int status = FSST_ERROR_OK;
	fs_dir_t kv_dir;
	struct dirent dir_ent
	char full_path_key[FSST_MAX_KEY_LEN+FSST_PATH_NAME_SIZE+1] = {0};


	_mutex.lock();
	if (false == _is_initialized) {
		status = FSST_ERROR_NOT_INITIALIZED;
		goto exit_point;
	}

	_fs->dir_open(&kv_dir, _cfg_fs_path);

	while (_fs->dir_read(kv_dir, &dir_ent) !=0 ) {
		if (dir_ent.d_type != DT_REG) {
			tr_error("KV_DIR should contain only Regular File - %s", dir_ent.d_name);
		}
		_build_full_path_key(full_path_key, dir_ent.d_name);
		_fs->remove(full_path_key);
	}

    status = _fs->remove(_cfg_fs_path);
    if (status == 0) {
    	_num_keys = 0;
    }
    else {
    	tr_error("Failed to remove empty KV Dir: %s", _cfg_fs_path);
    }
    _fs->dir_close(kv_dir);

exit_point:
	_mutex.unlock();
	return status;
}


// Core API
int FileSystemStore::set(const char *key, const void *buffer, size_t size, uint32_t create_flags)
{
	int status = FSST_ERROR_OK;
	set_handle_t handle;

	_mutex.lock();
	if (false == _is_initialized) {
		status = FSST_ERROR_NOT_INITIALIZED;
		goto exit_point;
	}

	strncat(full_path_key, key, FSST_MAX_KEY_LEN);
	full_path_key[(FSST_MAX_KEY_LEN+FSST_PATH_NAME_SIZE)] = '\0';

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

	_mutex.unlock();
	return status;
}


int FileSystemStore::_verify_key_file(const char *key, key_metadata_t *key_metadata, fs_file_t *kv_file)
{
	int status = FSST_ERROR_OK;

	char full_path_key[FSST_PATH_NAME_SIZE+FSST_MAX_KEY_LEN+1] = {0};
	_build_full_path_key(full_path_key, key);

	int file_size = 0;

	if (0 !=_fs->file_open(kv_file, full_path_key, O_RDONLY)) {
	  tr_error("Couldn't read: %s", full_path_key);
	  status = FSST_ERROR_FS_OPERATION_FAILED;
	  // TBD handle error scenario..?
	  goto exit_point;
	}

	file_size = _fs->file_size(*kv_file);

    //Read Metadata
	_fs->file_read(*kv_file, key_metadata, sizeof(key_metadata_t));

	if ((key_metadata->magic != FSST_MAGIC) ||
    	((file_size - key_metadata->metadata_size) != key_metadata->data_size) ||
    	(key_metadata->revision > FSST_REVISION) ) {
    	status = FSST_ERROR_CORRUPTED_DATA;
    	goto exit_point;
    }

exit_point:
    return status;
}

int FileSystemStore::get(const char *key, void *buffer, size_t buffer_size, size_t *actual_size, size_t offset = 0)
{
	int status = FSST_ERROR_OK;
	set_handle_t handle;
	fs_file_t kv_file = NULL;
	size_t value_actual_size = 0;

	_mutex.lock();
	if (false == _is_initialized) {
		status = FSST_ERROR_NOT_INITIALIZED;
		goto exit_point;
	}

	key_metadata_t key_metadata;

	if (FSST_ERROR_OK != _verify_key_file(key, &key_metadata, &kv_file) {
		tr_error("File Verification Failed: %s", key);
		status = FSST_ERROR_CORRUPTED_DATA;
		goto exit_point;
	}

	// Actual size is the minimum of buffer_size and remainder of data in file (data_size-offset)
	value_actual_size = buffer_size;
	if ((key_metadata.data_size-offset) < buffer_size) {
		value_actual_size = key_metadata.data_size - offset;
	}

	if (actual_size != NULL) {
		*actual_size = value_actual_size;
	}

	_fs->file_seek(kv_file, key_metadata.metadata_size + offset, SEEK_SET);
	// Read remainder of data
	_fs->file_read(kv_file, buffer, value_actual_size);

exit_point:
	if (kv_file != NULL) {
		_fs->file_close(kv_file);
	}
	_mutex.unlock();

	return status;
}

int FileSystemStore::get_info(const char *key, info_t *info)
{
	int status = FSST_ERROR_OK;
	set_handle_t handle;

	char full_path_key[FSST_PATH_NAME_SIZE+FSST_MAX_KEY_LEN+1] = {0};
	_build_full_path_key(full_path_key, key);
	fs_file_t kv_file = NULL;

	_mutex.lock();
	if (false == _is_initialized) {
		status = FSST_ERROR_NOT_INITIALIZED;
		goto exit_point;
	}

	key_metadata_t key_metadata;

	if (FSST_ERROR_OK != _verify_key_file(key, &key_metadata, &kv_file) {
		tr_error("File Verification Failed: %s", key);
		status = FSST_ERROR_CORRUPTED_DATA;
		goto exit_point;
	}

    info.size = key_metadata.data_size;
    infro.flags = key_metadata.user_flags;

exit_point:
	if (kv_file != NULL) {
		_fs->file_close(kv_file);
	}
	_mutex.unlock();

	return status;
}

int FileSystemStore::remove(const char *key)
{
	char full_path_key[FSST_PATH_NAME_SIZE+FSST_MAX_KEY_LEN+1] = {0};
	_build_full_path_key(full_path_key, key);

	_mutex.lock();
	if (false == _is_initialized) {
		return FSST_ERROR_NOT_INITIALIZED;
	}

    int status = _fs->remove(full_path_key);
    if (status == 0) {
    	_num_keys--;
    }
	_mutex.unlock();
	return status;
}

// Incremental set API
int FileSystemStore::set_start(set_handle_t *handle, const char *key, size_t final_data_size, uint32_t create_flags)
{
	char full_path_key[FSST_PATH_NAME_SIZE+FSST_MAX_KEY_LEN+1] = {0};

	fs_file_t kv_file;
	if (0 !=_fs->file_open(&kv_file, full_path_key, O_RDONLY)) {
		if (_num_keys == _max_keys) {
			return FSST_ERROR_MAX_KEYS_REACHED;
		}
		_num_keys++;
	}
	else {
		_fs->file_close(kv_file);
	}

	if (0 !=_fs->file_open(&kv_file, full_path_key, O_WRONLY)) {
		return FSST_ERROR_FS_OPERATION_FAILED;
	}

	inc_set_handle_t *set_handle = new inc_set_handle_t;
	set_handle->create_flags = create_flags;
	set_handle->data_size = final_data_size;
	set_handle->key = new char[strnlen(key, FSST_MAX_KEY_LEN)];
    strncpy(set_handle->key, key, FSST_MAX_KEY_LEN);
    set_handle->key[FSST_MAX_KEY_LEN] = '\0';

    *handle = (set_handle_t *)set_handle;

    key_metadata_t key_metadata;

    key_metadata.magic = FSST_MAGIC;
    key_metadata.metadata_size = sizeof(key_metadata_t);
    key_metadata.revision = FSST_REVISION;
    key_metadata.user_flags = create_flags;
    key_metadata.data_size = final_data_size;

    _fs->file_write(kv_file, &key_metadata, sizeof(key_metadata_t));

	_fs->file_close(kv_file);
    return FSST_ERROR_OK;
}

int FileSystemStore::set_add_data(set_handle_t handle, const void *value_data, size_t data_size)
{
	char full_path_key[FSST_PATH_NAME_SIZE+FSST_MAX_KEY_LEN+1] = {0};
	inc_set_handle_t *set_handle = (inc_set_handle_t *)handle;
	_build_full_path_key(full_path_key, set_handle->key);


	fs_file_t kv_file;
	if (0 !=_fs->file_open(&kv_file, full_path_key, O_RDONLY)) {
		return FSST_ERROR_FILE_NOT_FOUND;
	}
	_fs->file_close(kv_file);

	if (0 !=_fs->file_open(&kv_file, full_path_key, O_APPEND)) {
		return FSST_ERROR_FS_OPERATION_FAILED;
	}

    _fs->file_write(kv_file, value_data, data_size);
	_fs->file_close(kv_file);
    return FSST_ERROR_OK;

}

int FileSystemStore::set_finalize(set_handle_t handle)
{
	int status = FSST_ERROR_OK;
	inc_set_handle_t *set_handle = (inc_set_handle_t *)handle;

	if (set_handle == NULL) {
		return FSST_ERROR_INVALID_INPUT;
	}

	if (set_handle->key == NULL) {
		status = FSST_ERROR_INVALID_INPUT;
	}
	else {
		delete set_handle->key;
	}

	delete set_handle;
	return status;
}

// Key iterator
int FileSystemStore::iterator_open(iterator_t *it, const char *prefix = NULL)
{
    int status = FSST_ERROR_OK;
    DIR *kv_dir;

	_mutex.lock();
	if (false == _is_initialized) {
		status = FSST_ERROR_NOT_INITIALIZED;
		goto exit_point;
	}
    key_iterator_handle_t *key_it = new key_iterator_handle_t;

    key_it->prefix = NULL;
    if (prefix != NULL) {
    	key_it->prefix = new char[FSST_MAX_KEY_LEN+1];
    	strncpy(key_it->prefix, prefix, FSST_MAX_KEY_LEN);
    	key_it->prefix[FSST_MAX_KEY_LEN] = '\0';
    }


    fs_dir_t kv_dir;

    if (_fs->dir_open(&kv_dir, _cfg_fs_path) != 0) {
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
	DIR *kv_dir;
	struct dirent *kv_dir_ent;
	struct stat dir_buf;
	int kv_dir_ent_valid = 0;
	int status = FSST_ERROR_NOT_FOUND;
	char *key_ptr = NULL;

	_mutex.lock();
	if (false == _is_initialized) {
		status = FSST_ERROR_NOT_INITIALIZED;
		goto exit_point;
	}

	key_iterator_handle_t *key_it = (key_iterator_handle_t *)it;
	kv_dir = key_it->dir_handle;

	_fs->dir_read(kv_dir, &kv_dir_ent) !=0

	while (dir_read(kv_dir, &kv_dir_ent) !=0 ) {
		if (kv_dir_ent.d_type != DT_REG) {
			tr_error("KV_DIR should contain only Regular File - %s", dir_ent.d_name);
		}

		//_build_full_path_key(full_path_key, kv_dir_ent.d_name); //Is it required or name is already full path?

		//if (_strip_full_path_from_key(kv_dir_ent.d_name, &key_ptr) != 0) {
//			tr_error("Found filed in KV Dir with ");
	//	}

		if ( (key_it->prefix == NULL) ||
			 (strncmp(kv_dir_ent->d_name, key_it->prefix, strnlen(key_it->prefix, FSST_MAX_KEY_LEN)) != 0) ) {
			strncpy(key, kv_dir_ent->d_name, FSST_MAX_KEY_LEN);
			key[FSST_MAX_KEY_LEN] = '\0';
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

	_fs->dir_close(key_it->dir_handle);

	delete key_it;

exit_point:
	_mutex.unlock();
	return status;
}

} //namespace mbed

