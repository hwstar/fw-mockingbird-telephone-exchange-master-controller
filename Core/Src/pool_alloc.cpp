#include "top.h"
#include "util.h"
#include "logging.h"
#include "err_handler.h"
#include "pool_alloc.h"



namespace Pool_Alloc {

static const char *TAG = "poolalloc";


/*
 * Initialize a memory pool
 */
void Pool_Alloc::pool_init(void *memory_pool, uint32_t object_size, uint32_t num_objects) {
	this->_block_begin = memory_pool;
	this->_object_size = object_size;
	this->_num_objects = num_objects;
	this->_block_size = object_size * num_objects;
	this->_objects_allocated = 0;

	/* Mutex attributes */
	static const osMutexAttr_t pool_allocator_mutex_attr = {
		"PoolAllocatorMutex",
		osMutexRecursive | osMutexPrioInherit,
		NULL,
		0U
	};


	/* Create the lock mutex */

	this->_lock = osMutexNew(&pool_allocator_mutex_attr);
	if (this->_lock == NULL) {
		POST_ERROR(Err_Handler::EH_LCE);
	}



	/* Initialize the memory pool */
	Utility.memset(memory_pool,0 , this->_block_size);
	Free_Object_Type *current_object = reinterpret_cast<Free_Object_Type *>(memory_pool);
	/* Initialize next pointers up to num_objects - 1. The next field of the last block needs to have a NULL pointer in it. */
	for(uint32_t index = 0; index < num_objects ; index++) {
		if(index != num_objects - 1) { /* If not the last object */
			/* Set pointer to next object */
			current_object->next = reinterpret_cast<Free_Object_Type *>(reinterpret_cast<uint8_t *>(current_object) + object_size);
		}
		/* Set the magic number to detect overwrites */
		current_object->magic = ALLOC_MAGIC;
		current_object = current_object->next;

	}
	/* Point to first object in block */
	this->_alloc_ptr = this->_block_begin;
}

/*
 * Allocate an object from the memory pool.
 */

void *Pool_Alloc::allocate_object(void) {

	if((!this->_block_begin) || (!this->_alloc_ptr)) {
		POST_ERROR(Err_Handler::EH_AOOM);
	}

	osMutexAcquire(this->_lock, osWaitForever); /* Get the lock */

	void *allocated_object = this->_alloc_ptr;
	/* Move allocation pointer to next free object */
	this->_alloc_ptr = reinterpret_cast<void *>((reinterpret_cast<Free_Object_Type *>(this->_alloc_ptr)->next));

	this->_objects_allocated++;

	osMutexRelease(this->_lock); /* Release the lock */

	/* Check the magic number */
	if((reinterpret_cast<Free_Object_Type *>(allocated_object))->magic != ALLOC_MAGIC) {
		POST_ERROR(Err_Handler::EH_MCD);
	}

	/* Zero out the object */
	Utility.memset(allocated_object, 0, this->_object_size);

	/* Return the object */
	return allocated_object;

}


/*
 * Deallocate an object and return it to the memory pool.
 */

void Pool_Alloc::deallocate_object(void *object) {

	Free_Object_Type *freed_object = reinterpret_cast<Free_Object_Type *>(object);

	osMutexAcquire(this->_lock, osWaitForever); /* Get the lock */

	/* Set the next pointer to the current allocation pointer */
	freed_object->next = reinterpret_cast<Free_Object_Type *>(this->_alloc_ptr);
	freed_object->magic = ALLOC_MAGIC;
	/* Set the allocation pointer to the freed object */
	this->_alloc_ptr = object;

	this->_objects_allocated--;

	osMutexRelease(this->_lock); /* Release the lock */
}

/*
 * Return the number of objects allocated in the pool
 */

uint32_t Pool_Alloc::get_num_allocated_objects(void) {
	return this->_objects_allocated;
}


} /* End Namespace Pool_Alloc */
