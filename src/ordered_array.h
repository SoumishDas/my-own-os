// ordered_array.h -- Interface for creating, inserting and deleting
//                    from ordered arrays.
//                    Written for JamesM's kernel development tutorials.

/*
 * WHY THE HEAP NEEDS THIS
 * -----------------------
 * The heap can contain many free regions ("holes") of different sizes.  This
 * container stores a pointer to every hole header and keeps those pointers
 * sorted by the comparison callback.  kheap.c compares header->size, so its
 * index runs from the smallest hole to the largest.  Allocation walks that
 * list and stops at the first usable hole: a simple best-fit policy.
 *
 * The backing storage has a fixed capacity.  Insertion shifts later pointers
 * right; removal shifts them left.  Both operations are O(n), but the design is
 * compact and easy to bootstrap without a complicated tree allocator.
 */

#ifndef ORDERED_ARRAY_H
#define ORDERED_ARRAY_H

#include "common.h"

/**
   This array is insertion sorted - it always remains in a sorted state (between calls).
   It can store anything that can be cast to a void* -- so a u32int, or any pointer.
**/
typedef void* type_t;
/**
   A predicate should return nonzero if the first argument is less than the second. Else 
   it should return zero.
**/
typedef s8int (*lessthan_predicate_t)(type_t,type_t);
typedef struct
{
    type_t *array;                  // Backing memory containing untyped pointers.
    u32int size;                    // Number of currently valid entries.
    u32int max_size;                // Fixed capacity of the backing memory.
    lessthan_predicate_t less_than; // Callback that defines sorted order.
    u8int owns_storage;             // 1 only when create_ordered_array allocated array.
} ordered_array_t;

/**
   A standard less than predicate.
**/
s8int standard_lessthan_predicate(type_t a, type_t b);

/**
   Create an ordered array whose backing storage comes from kmalloc().
**/
ordered_array_t create_ordered_array(u32int max_size, lessthan_predicate_t less_than);
/*
 * Construct the same abstraction directly over memory beginning at addr.
 * The heap uses this form so its index occupies the first part of heap space.
 */
ordered_array_t place_ordered_array(void *addr, u32int max_size, lessthan_predicate_t less_than);

/**
   Destroy an ordered array.
**/
void destroy_ordered_array(ordered_array_t *array);

/**
   Insert an item at its sorted position, shifting later pointers right.
**/
void insert_ordered_array(type_t item, ordered_array_t *array);

/**
   Lookup the item at index i.
**/
type_t lookup_ordered_array(u32int i, ordered_array_t *array);

/**
   Delete item i, shifting later pointers left to fill the gap.
**/
void remove_ordered_array(u32int i, ordered_array_t *array);

#endif // ORDERED_ARRAY_H
