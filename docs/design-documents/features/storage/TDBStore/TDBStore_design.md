# TDBStore in Mbed OS

- [TDBStore in Mbed OS](#tdbstore-in-mbed-os)
    + [Revision history](#revision-history)
- [Introduction](#introduction)
    + [Overview and background](#overview-and-background)
    + [Requirements and assumptions](#requirements-and-assumptions)
- [System architecture and high-level design](#system-architecture-and-high-level-design)
  * [Design basics](#design-basics)
    + [Sequential writes](#sequential-writes)
    + [Memory layout and areas](#memory-layout-and-areas)
    + [Garbage collection](#garbage-collection)
    + [RAM Table](#ram-table)
- [Detailed design](#detailed-design)
    + [Class header](#class-header)
    + [Important data structures](#important-data-structures)
    + [Initialization and reset](#initialization-and-reset)
    + [Core APIs](#core-apis)
    + [Incremental set APIs](#incremental-set-apis)
    + [Key iterator APIs](#key-iterator-apis)
- [Usage scenarios and examples](#usage-scenarios-and-examples)
    + [Standard usage of the class](#standard-usage-of-the-class)
- [Other information](#other-information)
    + [Open issues](#open-issues)


### Revision history

| Revision 	| Date           	| Authors                                                	| Mbed OS version 	| Comments         	|
|----------	|----------------	|--------------------------------------------------------	|-----------------	|------------------	|
| 1.0      	| 16 September 2018	| David Saada ([@davidsaada](https://github.com/davidsaada/)) 	| 5.11+           	| Initial revision 	|

# Introduction

### Overview and background

TDBStore (Tiny Database Storage) is a lightweight module aimed for storing data on a flash storage. It is part of of the [KVStore](../KVStore/KVStore_design.md) class family, meaning that it supports the get/set interface. It is designed to optimize performance (speed of access), reduce wearing of the flash and to minimize storage overhead. It is also resilient to power failures.

### Requirements and assumptions

This feature requires a flash based block device such as `FlashIAPBlockDevice` or `SpifBlockDevice`. It can work on top of block devices that don't need erasing before writes, such as `HeapBlockDevice` or `SDBlockDevice`, but requires a flash simulator layer for this purpose, like the one offered by `FlashSimBlockDevice`. 

# System architecture and high-level design

## Design basics

TDBStore includes the following design basics:
- Sequential writes: All writes are made sequentially on the physical storage as records, superseding the previous ones for the same key.  
- Memory layout - areas: The physical storage is divided equally into two areas - active and standby. All writes are made to the end of the active area's free space. When the active area is exhausted, a garbage collection process is invoked, copying only the up to date values of all keys to the standby area, and turning it active.
- RAM table: Indexes all keys in RAM, thus allowing fast access to their records in the physical storage.   

### Sequential writes
All writes are made sequentially on the physical storage as records, superseding the previous ones for the same key. Each data record is written right after the last written one. If a key is updated, a new record with this key is written, overriding the previous value of this key. If a key is deleted, a new record with a "deleted" flag is added.
Writes expect the storage to be erased. However, TDBStore takes the "erase as you go" approcah, meaning that when it crosses a sector boundary, it checks whether the next sector is erased, and if not - it gets erased. This saves a lot of time during initialization and garbage collection (see below). 

### Memory layout and areas
![TDBStore Areas](./TDBStore_areas.jpg)

Each key is stored in a separate record on the active area. The first record in the area is the master record. Its main purpose is to hold an area version, protecting us against the case we have two valid areas (can happen in the extreme cases of power failures). 


![TDBStore Record](./TDBStore_record.jpg)

Record key and data are preceded by a 24-byte header. Fields are:

- Magic: A constant value, for quick validity checking
- Header size: Size of header
- Revision: TDBStore revision (currently 1)
- User flags: Flags received from user. Currently only write once is dealt with (others are ignored)
- Internal flags: Internal TDBStore flags (currently only includes deleted flag)
- Key size: Size of key 
- Data size: Size of data
- CRC: A 32-bit CRC, caluclated on header (except CRC), key and data
- Programming size pad: Padding to the storage programming size  

### Garbage collection
Garbage collection (GC) is the process of compacting the records stored in the active area to the standby one, by copying only the most recent values of all the keys (without the ones marked as deleted). Then, the standby area becomes the active one and the previously active area is erased (not fully, only its first sector).  
GC is invoked in the following cases:
1. When the active area is exhausted.
2. During initialization, when a corruption is found while scanning the active area. In this case, GC is performed up to the record preceding the corruption.

### Reserved space
Active area includes a fixed and small reserved space. This space is used for a quick storage and extraction of a write once data (like device key). Its size is 32 bytes, aligned up to the underlying block device. Once it is written, it can't be modified. It is also copied between the areas during garbage collection process.

### RAM Table

All keys are indexed in memory using a RAM table. Key names are represented by a 32-bit hash. The table includes the hash (and sorted by it) and the offset to the key record in the block device. This allows both fast searching in the table as well as a low memoory footprint.

![TDBStore RAM Table](./TDBStore_ram_table.jpg)

Key names may produce duplicate hash values. This is OK, as the hash is only used for fast access to the key, and the key needs to be verified when accessing the storage. If the key doesn't match, we'll move to the next duplicate in the table. 

 
# Detailed design

TDBStore fully implements the KVStore interface over a block device. Due to the fact it may write to the block device in program units that don't have to match the underlying device program units, it should use a `BufferedBlockDevice` for that purpose.

![TDBStore Class Hierarchy](./TDBStore_class_hierarchy.jpg)

Functionality, as defined by KVStore, includes the following:
- Initialization & reset
- Core actions: get, set & remove
- Incremental set actions
- Iterator actions

### Class header

TDBStore has the following header:

```C++
class TDBStore : KVStore {

public:
    TDBSTore(size_t max_keys, BlockDevice *bd = 0);
    virtual ~TDBSTore();
    	 
    // Initialization and reset
    virtual int init();
    virtual int deinit();
    virtual int reset();

    // Core API
    virtual int set(const char *key, const void *buffer, size_t size, uint32_t create_flags);
    virtual int get(const char *key, void *buffer, size_t buffer_size, size_t *actual_size, size_t offset = 0);
    virtual int get_info(const char *key, info_t *info);
    virtual int remove(const char *key);
 
    // Incremental set API
    virtual int set_start(set_handle_t *handle, const char *key, size_t final_data_size, uint32_t create_flags);
    virtual int set_add_data(set_handle_t handle, const void *value_data, size_t data_size);
    virtual int set_finalize(set_handle_t handle);
 
    // Key iterator
    virtual int iterator_open(iterator_t *it, const char *prefix = NULL);
    virtual int iterator_next(iterator_t it, char *key, size_t key_size, size_t *actual_key_size);
    virtual int iterator_close(iterator_t it);
    
    // Reserved space APIs
    virtual int reserved_space_set(void *data);
    virtual int reserved_space_get(void *data);
    
private:
    Mutex _mutex;
    void *_ram_table;
    size_t *_max_keys;
    size_t *_num_keys;
    BlockDevice *_bd;
    bd_addr_t _free_space_offset;
    BufferedBlockDevice *_buff_bd;
    bool _is_initialized;
    int _active_area;
    
    // Important internal functions
    
    // find record offset in flash
    int find_record(const char *key, uint32_t *hash, bd_size_t *bd_offset, size_t ram_table_ind);
    
    // garbage collection
    int garbage_collection(const char *key, const void *buffer, size_t size, uint32_t create_flags);
}
```

### Important data structures

```C++
// RAM table entry
typedef struct {
    uint32_t  hash;
    bd_size_t bd_offset;
} ram_table_entry_t;

// Record header
typedef struct {
    uint32_t magic;
    uint16_t header_size;
    uint16_t revision;
    uint32_t user_flags;
    uint16_t int_flags;
    uint16_t key_size;
    uint32_t data_size;
    uint32_t crc;
} record_header_t;

// incremental set handle
typedef struct {
    bd_size_t bd_base_offset;
    bd_size_t bd_curr_offset;
    size_t key_size;
    size_t data_size;
    uint32_t create_flags;
    uint32_t crc;
} inc_set_handle_t;

// iterator handle
typedef struct {
    size_t ram_table_ind;
    char *prefix;
} key_iterator_handle_t;
```


### Initialization and reset

**init function**

Header:  
`virtual int init();`

Pseudo code:  
- if `_is_initialized` return OK
- Take `_mutex` 
- Allocate `_ram_table` as an array of `_num_keys`  
- Allocate `_buff_bd` with `_bd` as the underlying block device and initialize it  
- Check validity of master records on both areas
- If one is valid, set its area as `_active_area`
- If both are valid, set the one area whose master record has the higher version as `_active_area`. Erase first sector of the other one.
- If none are valid, set area 0 as `_active_area`, and write master record with version 0.
- Traverse active area until reaching an erased sector
	- Read current record and check its validity (calculte CRC)
	- If not valid, perform garbage collection and exit loop
	- Advance `_free_space_offset`
	- Call `find_record` function to calculate hash and find key
	- If not found, add new RAM table entry with current hash
	- Update position of key in RAM table
- Set `_is_initialized` to true
- Release `_mutex`     
  
**deinit function**

Header:  
`virtual int deinit();`

Pseudo code:  
- if not `_is_initialized` return OK
- Take `_mutex` 
- Deinitialize `_buff_bd` and free it  
- Free `_ram_table`  
- Set `_is_initialized` to false
- Release `_mutex`   

**reset function**

Header:  
`virtual int reset();`

Pseudo code:  
- Take `_mutex` 
- Erase first sector in both areas  
- Set `_active_area` to 0
- Write a master record with version 0
- Set `_free_space_offset` to end of master record
- Set `_num_keys` to 0
- Release `_mutex`   

### Core APIs

**set function**

Header:  
`virtual int set(const char *key, const void *buffer, size_t size, uint32_t create_flags);`

Pseudo code:  
- if not `_is_initialized` return "not initialized" error
- Call `set_start` with all fields and a local `set_handle_t` variable
- Call `set_add_data` with `buffer` and `size`
- Call `set_finalize`
- Return OK

**get function**

Header:  
`virtual int get(const char *key, void *buffer, size_t buffer_size, size_t *actual_size, size_t offset = 0);`

Pseudo code:  
- if not `_is_initialized` return "not initialized" error
- Take `_mutex` 
- Call `find_record` to find record in storage
- If not found, return "not found" error
- Read header and calculate CRC on it
- Update CRC with key (if offset is 0)
- Read data into user buffer, starting offset. Size is minimum of buffer size and remainder of data
- If offset is 0
	- Update CRC with buffer
	- Compare calculate CRC with header CRC. Return "data corrupt" error if different.
- Release `_mutex`  
- Return OK   

**get_info function**

Header:  
`virtual int get_info(const char *key, info_t *info);`

Pseudo code:  
- if not `_is_initialized` return "not initialized" error
- Take `_mutex` 
- Call `find_record` to find record in storage
- If not found, return "not found" error
- Read header 
- Copy relevant fields from header into structure
- Release `_mutex`  
- Return OK   

**remove function**

Header:  
`virtual int remove(const char *key);`

Pseudo code:  
- if not `_is_initialized` return "not initialized" error
- Take `_mutex` 
- Call `find_record` to find record in storage
- If not found, return "not found" error
- If `int_flags` field in header includes write once flag, return "write once" error
- Fill header with all fields (except CRC), set delete flag in internal flags
- Calculate CRC on header and name and set field in header
- Program header
- Program post header pad with 0xAA if needed
- Program key
- Program post data header pad with 0xAA if needed
- Call `sync` on buffered block device
- Advance `_free_space_offset`
- Remove entry from ram table
- Release `_mutex`     
- Return OK

### Incremental set APIs

**set_start function**

Header:  
`virtual int set_start(set_handle_t *handle, const char *key, size_t final_data_size, uint32_t create_flags);`

Pseudo code:
- Take `_mutex`  
- Check if final size fits in free space, if not call `garbage_collection`
- Call `find_record` to find record in storage
- If found and `int_flags` field in header includes write once flag, return "write once" error
- Allocate an `inc_set_handle_t` structure into `handle`
- Calculate hash on `key` and update in `handle`
- Update `bd_base_offset` in handle to `_free_space_offset`
- Update a `record_header_t` structure with all relevant values
- Update `key_size`, `data_size` and `create_flags` in `handle`
- Calculate crc on header
- Update crc with key and update in `handle`
- Program key in position after header
- Advance `_free_space_offset` and update in `bd_curr_offset` field in handle
- Set `_free_space_offset` and update in `bd_curr_offset` field in handle
- Call `find_record` to calculate hash and find record in storage (with null key and current hash)
- If not found, add entry in ram table
- Update offset in ram table

**set_add_data function**

Header:  
`virtual int set_add_data(set_handle_t handle, const void *value_data, size_t data_size);`

Pseudo code:
- Calculate crc on `value_data` and update in handle
- Program `value_data` from `bd_curr_offset`
- Advance `bd_curr_offset`

**set_finalize function**

Header:  
`virtual int set_finalize(set_handle_t handle);`

Pseudo code:
- Program 0xAA as pad in `bd_curr_offset` from handle and advance `_free_space_offset`
- Update a `record_header_t` structure with all relevant values
- Program header at `bd_base_offset` from handle with pads
- Call `sync` on buffered block device
- Free `handle`
- Release `_mutex`

### Key iterator APIs

**iterator_open function**

Header:  
`virtual int iterator_open(iterator_t *it, const char *prefix = NULL);`

Pseudo code:
- Take `_mutex`  
- Allocate a `key_iterator_handle_t` structure into `it`
- Set `ram_table_ind` field in iterator to 0
- Duplicate `prefix` into same field in iterator
- Release `_mutex`

**iterator_next function**

Header:  
`virtual int iterator_next(iterator_t it, char *key, size_t key_size, size_t *actual_key_size);`

Pseudo code:
- Take `_mutex`  
- While `ram_table_ind` field in iterator smaller than `_num_keys`
	- Read key pointed to by ram table in `ram_table_ind` into a local variable
	- If name matches prefix
		- Advance `ram_table_ind` field in iterator
		- Copy name to `key` and return OK
	- Advance `ram_table_ind` field in iterator 
- Return "not found" error
- Release `_mutex`

**iterator_close function**

Header:  
`virtual int iterator_close(iterator_t it);`

Pseudo code:
- Release `prefix` field in iterator and structure allocated at `it`


### Reserved space

**reserved_space_set function**

Header:  
`virtual int reserved_space_set(void *data);`

Pseudo code:  
- Check if reserved space is not empty, if it is, return a "reserved space programmed error"
- Copy `data` contents to reserved space location 

**reserved_space_get function**

Header:  
`virtual int reserved_space_get(void *data);`

Pseudo code:  
- Copy contents from reserved space location `data`  


# Usage scenarios and examples

### Standard usage of the class

Following example code shows standard usage of the TDBStore class 

**Standard usage example**

```C++
// Underlying block device. Here, SPI Flash is fully used.
// One can use SlicingBlockDevice if we want a partition.
SPIFBlockDevice bd(PTE2, PTE4, PTE1, PTE5);

// Instantiate tdbstore with our block device and 64 as max number of keys
TDBStore tdbstore(64, &bd);

int res;

// Initialize tdbstore
res = tdbstore.init();

// Add "Key1"
const char *val1 = "Value of key 1";
const char *val2 = "Updated value of key 1";
res = tdbstore.set("Key1", val1, sizeof(val1), 0);
// Update value of "Key1"
res = tdbstore.set("Key1", val2, sizeof(val2), 0);

uint_8 value[32];
size_t actual_size;
// Get value of "Key1". Value should return the updated value.
res = tdbstore.get("Key1", value, sizeof(value), &actual_size);

// Remove "Key1"
res = tdbstore.remove("Key1");

// Incremental write, if need to generate large data with a small buffer
const int data_size = 1024;
char buf[8];

KVSTore::set_handle_t handle;
res = tdbstore.set_start(&handle, "Key2", data_size, 0);
for (int i = 0; i < data_size / sizeof(buf); i++) {
	memset(buf, i, sizeof(buf));
	res = tdbstore.set_add_data(handle, buf, sizeof(buf));
}
res = tdbstore.set_finalize(handle);

// Iterate over all keys starting with "Key"
res = 0;
KVSTore::iterator_t it;
tdbstore.iterator_open(&it, "Key*");
char key[KVSTore::KV_MAX_KEY_LENGTH];
size_t actual_key_size;
while (!res) {
    res = tdbstore.iterator_next(&it, key, sizeof(key), &actual_key_size);
}
res = tdbstore.iterator_close(&it);

// Deinitialize TDBStore
res = tdbstore.deinit();
```
# Other information

### Open issues

- Do we use TLS API for hash calculation, or do we have simpler function (to reduce code size), maybe the CRC?
- Need to figure a way to prevent mutex abuse in incremental set APIs. 
