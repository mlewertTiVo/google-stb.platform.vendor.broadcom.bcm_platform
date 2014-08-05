/*=============================================================================
Copyright (c) 2011 Broadcom Europe Limited.
All rights reserved.

Project  :  Containers
Module   :  Containers Core
File     :  $RCSfile: $
Revision :  $Revision: $

FILE DESCRIPTION
Interface for handling sorted lists of data of arbitrary size.

Static, read-only (constant) lists can be defined by using the
VC_CONTAINERS_STATIC_LIST macro.
=============================================================================*/

#ifndef _VC_CONTAINERS_LIST_H_
#define _VC_CONTAINERS_LIST_H_

#include "containers/containers.h"

/** List entry comparison prototype.
 * Returns zero if items at a and b match, positive if a is "bigger" than b and
 * negative if a is "smaller" than b. */
typedef int (*VC_CONTAINERS_LIST_COMPARATOR_T)(const void *a, const void *b);

/** Sorted list type.
 * Storage type providing efficient insertion and search via binary sub-division. */
typedef struct vc_containers_list_tag
{
   uint32_t size;                               /**< Number of defined entries in list */
   uint32_t capacity;                           /**< Capacity of list, in entries, or zero for read-only */
   size_t entry_size;                           /**< Size of one entry, in bytes */
   VC_CONTAINERS_LIST_COMPARATOR_T comparator;  /**< Entry comparison function */
   void *entries;                               /**< Pointer to array of entries */
} VC_CONTAINERS_LIST_T;

/** Macro to generate a static, read-only list from an array and comparator */
#define VC_CONTAINERS_STATIC_LIST(L, A, C)  static VC_CONTAINERS_LIST_T L = { countof(A), 0, sizeof(*(A)), (VC_CONTAINERS_LIST_COMPARATOR_T)(C), A }


/** Create an empty list.
 * The list is created based on the details provided, minimum capacity one entry.
 *
 * \param The initial capacity in entries.
 * \param entry_size The size of each entry, in bytes.
 * \param comparator A function for comparing two entries.
 * \return The new list or NULL. */
VC_CONTAINERS_LIST_T *vc_containers_list_create(uint32_t capacity, size_t entry_size, VC_CONTAINERS_LIST_COMPARATOR_T comparator);

/** Destroy a list.
 * Has no effect on a static list.
 *
 * \param list The list to be destroyed. */
void vc_containers_list_destroy(VC_CONTAINERS_LIST_T *list);

/** Reset a list to be empty.
 * Has no effect on a static list.
 *
 * \param list The list to be reset. */
void vc_containers_list_reset(VC_CONTAINERS_LIST_T *list);

/** Insert an entry into the list.
 *
 * \param list The list.
 * \param new_entry The new entry to be inserted.
 * \param allow_duplicates Determines whether to insert or overwrite if there
 *    is an existing matching entry.
 * \return True if the entry has successfully been inserted, false if the list
 *    needed to be enlarged and the memory allocation failed. */
bool vc_containers_list_insert(VC_CONTAINERS_LIST_T *list, void *new_entry, bool allow_duplicates);

/** Find an entry in the list and fill in the result.
 * Searches for an entry in the list using the comparator and if found
 * overwrites the one passed in with the one found.
 *
 * \param list The list to search.
 * \param entry An entry with enough defined to find it in the list, filled in
 *    with the rest if found.
 * \return True if found, false if not. */
bool vc_containers_list_find_entry(const VC_CONTAINERS_LIST_T *list, void *entry);

/** Validates a list pointer.
 * Fields and contents of a list are checked and asserted to be correct. With a
 * large list this may be slow, so it is recommended only to call this in debug
 * builds.
 *
 * \param list The list to be validated. */
void vc_containers_list_validate(const VC_CONTAINERS_LIST_T *list);

#endif /* _VC_CONTAINERS_LIST_H_ */
