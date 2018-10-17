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


using namespace mbed;

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
FileSystemStore::FileSystemStore(size_t max_keys/*, FileSystem *fs = 0*/) : _max_keys(max_keys), _num_keys(0), _is_initialized(false)
{
	memset(_cfg_fs_path, 0, FSST_MAX_KEY_LEN);
	strncpy(_cfg_fs_path,"tbd_cfg_fs_path", FSST_MAX_KEY_LEN);
	_cfg_fs_path[FSST_MAX_KEY_LEN] = '\0';
}
    	 
// Initialization and reset
int FileSystemStore::init()
{
  DIR *kv_dir;
  struct dirent *kv_dir_ent;
  struct stat dir_buf;
  int kv_dir_ent_valid = 0;

  _mutex.lock();
  kv_dir = opendir(_cfg_fs_path);
  if (kv_dir == NULL) {
    tr_info("KV Dir: %s, doesnt exist - creating new.. ", _cfg_fs_path); //TBD verify ERRNO NOEXIST
    mkdir(_cfg_fs_path,/* which flags ? */); 
  }

  for (kv_dir_ent = readdir(kv_dir); kv_dir_ent != NULL; kv_dir_ent = readdir(kv_dir)) {
    kv_dir_ent_valid = stat(kv_dir_ent->d_name, &dir_buf);
    if (kv_dir_ent_valid < 0) {
      tr_error("Couldn't stat: %s", kv_dir_ent->d_name);
	  // TBD handle error scenario..?
    } else {
      _num_keys++;
    }
  }
  closedir(kv_dir);
  
  _is_initialized = true;
  _mutex.unlock();

  return FSST_ERROR_OK;

}

int FileSystemStore::_build_full_path_key(char *full_path_key_dst, const char *key_stc)
{
	strncpy(full_path_key_dst,_cfg_fs_path, FSST_MAX_KEY_LEN);
	strncat(full_path_key_dst, key_src, FSST_MAX_KEY_LEN);
	full_path_key_dst[(2*FSST_MAX_KEY_LEN)] = '\0';
	return 0;
}


int FileSystemStore::deinit()
{
	// Required ??
	//if (false == _is_initiailized) {
	//		return FSST_ERROR_OK
	//	}
	_mutex.lock();
    _is_initialized = true;
	_mutex.unlock();

}

int FileSystemStore::reset()
{
	int status = FSST_ERROR_OK;
	_mutex.lock();
	if (false == _is_initialized) {
		status = FSST_ERROR_NOT_INITIALIZED;
		goto exit_point;
	}
    status = remove(_cfg_fs_path);
    if (status == 0) {
    	_num_keys = 0;
    }

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
	full_path_key[(2*FSST_MAX_KEY_LEN)] = '\0';

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


int FileSystemStore::_verify_key_file(const char *key, key_metadata_t *key_metadata, FILE **input_file)
{
	int status = FSST_ERROR_OK;

	char full_path_key[FSST_MAX_KEY_LEN*2+1] = {0};
	_build_full_path_key(full_path_key, key);


	struct stat key_stat;
	int is_key_valid = 0;

	is_key_valid = stat(full_path_key, &key_stat);
	if (is_key_valid < 0) {
	  tr_error("Couldn't stat: %s", full_path_key);
	  status = FSST_ERROR_FILE_OPERATION_FAILED;
	  // TBD handle error scenario..?
	  goto exit_point;
	}

	int file_size = key_stat.st_size;

	*input_file = fopen(full_path_key, "r");
	if (NULL == *input_file) {
		tr_error("Couldn't read: %s", full_path_key);
		status = FSST_ERROR_FILE_OPERATION_FAILED;
		goto exit_point;
	}


    //Read Metadata
	fread(key_metadata, sizeof(key_metadata_t), 1, *input_file);

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
	FILE *input_file = NULL;


	_mutex.lock();
	if (false == _is_initialized) {
		status = FSST_ERROR_NOT_INITIALIZED;
		goto exit_point;
	}

	key_metadata_t key_metadata;

	if (FSST_ERROR_OK != _verify_key_file(key, &key_metadata, &input_file) {
		tr_error("File Verification Failed: %s", key);
		goto exit_point;
	}

	// Actual size is the minimum of buffer_size and remainder of data in file (data_size-offset)
	*actual_size = buffer_size;
	if ((key_metadata.data_size-offset) < buffer_size) {
		*actual_size = key_metadata.data_size - offset;
	}

	fseek(input_file, key_metadata.metadata_size + offset, SEEK_SET);
	// Read remainder of data
	fread(buffer, 1, *actual_size, input_file);

exit_point:
	if (input_file != NULL) {
		fclose(input_file);
	}
	_mutex.unlock();

	return status;
}

int FileSystemStore::get_info(const char *key, info_t *info)
{
	int status = FSST_ERROR_OK;
	set_handle_t handle;

	char full_path_key[FSST_MAX_KEY_LEN*2+1] = {0};
	_build_full_path_key(full_path_key, key);
	FILE *input_file = NULL;

	_mutex.lock();
	if (false == _is_initialized) {
		status = FSST_ERROR_NOT_INITIALIZED;
		goto exit_point;
	}

	key_metadata_t key_metadata;

	if (FSST_ERROR_OK != _verify_key_file(key, &key_metadata, &input_file) {
		tr_error("File Verification Failed: %s", key);
		goto exit_point;
	}

    info.size = key_metadata.data_size;
    infro.flags = key_metadata.user_flags;

exit_point:
	if (input_file != NULL) {
		fclose(input_file);
	}
	_mutex.unlock();

	return status;
}

int FileSystemStore::remove(const char *key)
{
	_mutex.lock();
	if (false == _is_initialized) {
		return FSST_ERROR_NOT_INITIALIZED;
	}
    int status = remove(key);
    if (status == 0) {
    	_num_keys--;
    }
	_mutex.unlock();
	return status;
}

// Incremental set API
int FileSystemStore::set_start(set_handle_t *handle, const char *key, size_t final_data_size, uint32_t create_flags)
{
	char full_path_key[FSST_MAX_KEY_LEN*2+1] = {0};

	_build_full_path_key(full_path_key, key);

	FILE *output_file = fopen(full_path_key, "r");
	if (NULL == output_file) {
		if (_num_keys == _max_keys) {
			return FSST_ERROR_MAX_KEYS_REACHED;
		}
		_num_keys++;
	}
	else {
		fclose(output_file);
	}

	output_file = fopen(full_path_key, "w");
	if (output_file == NULL) {
		return FSST_ERROR_FILE_OPERATION_FAILED;
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

    fwrite(&key_metadata, sizeof(key_metadata_t), 1, output_file);

    fclose(output_file);
    return FSST_ERROR_OK;
}

int FileSystemStore::set_add_data(set_handle_t handle, const void *value_data, size_t data_size)
{
	char full_path_key[FSST_MAX_KEY_LEN*2+1] = {0};
	inc_set_handle_t *set_handle = (inc_set_handle_t *)handle;
	_build_full_path_key(full_path_key, set_handle->key);


	FILE *output_file = fopen(full_path_key, "r");
	if (NULL == output_file) {
		return FSST_ERROR_FILE_NOT_FOUND;
	}
	fclose(output_file);

	output_file = fopen(full_path_key, "a");
	if (output_file == NULL) {
		return FSST_ERROR_FILE_OPERATION_FAILED;
	}

	fwrite(value_data, 1, data_size, output_file);
    fclose(output_file);

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

    kv_dir = opendir(_cfg_fs_path);
    if (kv_dir == NULL) {
      tr_error("KV Dir: %s, doesnt exist", _cfg_fs_path); //TBD verify ERRNO NOEXIST
      status = FSST_ERROR_NOT_FOUND;
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

	_mutex.lock();
	if (false == _is_initialized) {
		status = FSST_ERROR_NOT_INITIALIZED;
		goto exit_point;
	}

	key_iterator_handle_t *key_it = (key_iterator_handle_t *)it;
	kv_dir = key_it->dir_handle;
	kv_dir_ent = readdir(kv_dir);
	while (kv_dir_ent != NULL) {
		if ( (key_it->prefix == NULL) ||
			 (strncmp(kv_dir_ent->d_name, key_it->prefix, strnlen(key_it->prefix, FSST_MAX_KEY_LEN)) != 0) ) {
			strncpy(key, kv_dir_ent->d_name, FSST_MAX_KEY_LEN);
			key[FSST_MAX_KEY_LEN] = '\0';
			status = FSST_ERROR_OK;
			break;
		}
		kv_dir_ent = readdir(kv_dir);
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

	if (set_handle->key == NULL) {
		status = FSST_ERROR_INVALID_INPUT;
	}
	else {
		delete key_it->prefix;
	}

	delete key_it;

exit_point:
	_mutex.unlock();
	return status;
}


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


