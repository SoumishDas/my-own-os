// ordered_array.c -- Implementation for creating, inserting and deleting
//                    from ordered arrays.
//                    Written for JamesM's kernel development tutorials.

/*
 * This file manages only an array of pointers.  It never understands the
 * objects behind those pointers; less_than supplies that missing knowledge.
 * For the heap, each pointer refers to header_t and less_than compares sizes.
 */

#include "ordered_array.h"
#include "kheap.h"

s8int standard_lessthan_predicate(type_t a, type_t b)
{
    /* Useful when the pointer/integer values themselves define the order. */
    return ((u32int)a < (u32int)b)?1:0;
}

ordered_array_t create_ordered_array(u32int max_size, lessthan_predicate_t less_than)
{
    ASSERT(max_size > 0);
    ASSERT(max_size <= 0xFFFFFFFF / sizeof(type_t));
    ASSERT(less_than != 0);
    ordered_array_t to_ret;
    /* Reserve max_size pointer slots.  size remains zero until insertion. */
    to_ret.array = (void*)kmalloc(max_size*sizeof(type_t));
    /* Zeroing is not required for sorting, but makes unused slots predictable. */
    memset(to_ret.array, 0, max_size*sizeof(type_t));
    to_ret.size = 0;
    to_ret.max_size = max_size;
    to_ret.less_than = less_than;
    to_ret.owns_storage = 1;
    return to_ret;
}

ordered_array_t place_ordered_array(void *addr, u32int max_size, lessthan_predicate_t less_than)
{
    ASSERT(addr != 0);
    ASSERT(max_size > 0);
    ASSERT(max_size <= 0xFFFFFFFF / sizeof(type_t));
    ASSERT(less_than != 0);
    ordered_array_t to_ret;
    /*
     * Unlike create_ordered_array(), this does not allocate.  The caller has
     * already reserved max_size pointer slots beginning at addr.
     */
    to_ret.array = (type_t*)addr;
    memset(to_ret.array, 0, max_size*sizeof(type_t));
    to_ret.size = 0;
    to_ret.max_size = max_size;
    to_ret.less_than = less_than;
    to_ret.owns_storage = 0;
    return to_ret;
}

void destroy_ordered_array(ordered_array_t *array)
{
    ASSERT(array != 0);
    if (array->owns_storage && array->array != 0)
        kfree(array->array);
    array->array = 0;
    array->size = 0;
    array->max_size = 0;
    array->owns_storage = 0;
}

void insert_ordered_array(type_t item, ordered_array_t *array)
{
    ASSERT(array != 0);
    ASSERT(array->array != 0);
    /* Calling through a null comparison callback would jump to address zero. */
    ASSERT(array->less_than);
    /* The backing array has fixed capacity; never shift an item past its end. */
    ASSERT(array->size < array->max_size);

    /* Find the first existing entry which does not sort before the new item. */
    u32int iterator = 0;
    while (iterator < array->size && array->less_than(array->array[iterator], item))
        iterator++;
    /* Shift populated entries backward; never read the uninitialized end slot. */
    u32int destination = array->size;
    while (destination > iterator)
    {
        array->array[destination] = array->array[destination-1];
        destination--;
    }
    array->array[iterator] = item;
    array->size++;
}

type_t lookup_ordered_array(u32int i, ordered_array_t *array)
{
    ASSERT(array != 0);
    ASSERT(array->array != 0);
    /* Only [0, size) is populated, even though [0, max_size) is allocated. */
    ASSERT(i < array->size);
    return array->array[i];
}

void remove_ordered_array(u32int i, ordered_array_t *array)
{
    ASSERT(array != 0);
    ASSERT(array->array != 0);
    /* Only populated entries may be removed. */
    ASSERT(i < array->size);

    /*
     * Overwrite the removed entry with every later entry in succession.
     * Stop at size - 1: the final populated entry has no successor to copy.
     */
    while (i + 1 < array->size)
    {
        array->array[i] = array->array[i+1];
        i++;
    }
    /* One fewer slot is part of the populated prefix after shifting. */
    array->size--;
    array->array[array->size] = 0;
}
