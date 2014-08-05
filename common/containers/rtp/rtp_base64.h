/*=============================================================================
Copyright (c) 2011 Broadcom Europe Limited.
All rights reserved.

Project  :  Containers
Module   :  RTP: Real-time Transport Protocol
File     :  $RCSfile: $
Revision :  $Revision: $

FILE DESCRIPTION
Interface for decoding Base64 encoded data.
=============================================================================*/

#ifndef _RTP_BASE64_H_
#define _RTP_BASE64_H_

#include "containers/containers.h"

/** Returns the number of bytes encoded by the given Base64 encoded string.
 *
 * \param str The Base64 encoded string.
 * \param str_len The number of characters in the string.
 * \return The number of bytes that can be decoded. */
uint32_t rtp_base64_byte_length(const char *str, uint32_t str_len);

/** Decodes a Base64 encoded string into a byte buffer.
 *
 * \param str The Base64 encoded string.
 * \param str_len The number of characters in the string.
 * \param buffer The buffer to receive the decoded output.
 * \param buffer_len The maximum number of bytes to put in the buffer.
 * \return Pointer to byte after the last one converted, or NULL on error. */
uint8_t *rtp_base64_decode(const char *str, uint32_t str_len, uint8_t *buffer, uint32_t buffer_len);

#endif /* _RTP_BASE64_H_ */
