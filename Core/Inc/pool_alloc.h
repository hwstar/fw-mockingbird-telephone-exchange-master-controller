#pragma once
#include "top.h"


namespace Pool_Alloc {

const uint32_t ALLOC_MAGIC = 0x0d8a3cb2;

typedef struct free_object {
	uint32_t magic;
	struct free_object *next;
} Free_Object_Type;


class Pool_Alloc {
protected:
	osMutexId_t _lock;
	uint32_t _objects_allocated;
	uint32_t _object_size;
	uint32_t _num_objects;
	uint32_t _block_size;
	void *_block_begin;
	void *_alloc_ptr;

public:
	void pool_init(void *memory_pool, uint32_t object_size, uint32_t num_objects);
	void *allocate_object(void);
	void deallocate_object(void *object);
	uint32_t get_num_allocated_objects(void);
};



} /* End Namespace Pool_Alloc */
