/*==============================================================================
Copyright (c) 2011 Broadcom Europe Limited.
All rights reserved.

Project  :  Containers Library
Module   :  Containers Core

FILE DESCRIPTION
Definition of index utility for containers.
=============================================================================*/
#ifndef VC_CONTAINERS_INDEX_H
#define VC_CONTAINERS_INDEX_H

/** \file containers_index.h
 * Definition of index utilitie for containers.  Creates and maintains an
 * index of file offsets and times, and is able to suggest a file position
 * to seek to achieve a given time target.  Useful for container formats
 * that don't include an index.
 */

#include "containers/containers.h" 

struct VC_CONTAINER_INDEX_T;
typedef struct VC_CONTAINER_INDEX_T VC_CONTAINER_INDEX_T;

/**
 * Creates an index with a suggested number of entries.
 * @param  index   Pointer to created index will be filled here on success.
 * @param  length  Suggested length of index.
 * @return         Status code
 */
VC_CONTAINER_STATUS_T vc_container_index_create( VC_CONTAINER_INDEX_T **index, int length );


/**
 * Frees an index.
 * @param index  Pointer to valid index.
 * @return       Status code.
 */
VC_CONTAINER_STATUS_T vc_container_index_free( VC_CONTAINER_INDEX_T *index );


/**
 * Adds an entry to the index.  If the index is full then some stored records will be
 * discarded.
 * @param index        Pointer to a valid index.
 * @param time         Timestamp of new index entry.
 * @param file_offset  File offset for new index entry.
 * @return             Status code
 */
VC_CONTAINER_STATUS_T vc_container_index_add( VC_CONTAINER_INDEX_T *index, int64_t time, int64_t file_offset );


/**
 * Retrieves the best entry for the supplied time offset.
 * @param index        Pointer to valid index.
 * @param later        If true, the selected entry is the earliest retained entry with a greater or equal timestamp. 
 *                     If false, the selected entry is the latest retained entry with an earlier or equal timestamp.
 * @param time         The requested time.  On success, this is filled in with the time of the selected entry.
 * @param file_offset  On success, this is filled in with the file offset of the selected entry.
 * @param past         Set if the requested time is after the last entry in the index.
 * @return             Status code.
 */
VC_CONTAINER_STATUS_T vc_container_index_get( VC_CONTAINER_INDEX_T *index, int later, int64_t *time, int64_t *file_offset, int *past );

#endif /* VC_CONTAINERS_WRITER_UTILS_H */
