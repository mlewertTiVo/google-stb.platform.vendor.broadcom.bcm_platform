/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree under external/libvpx/libvpx. An additional intellectual property
 *  rights grant can be found in the file PATENTS.  All contributing project
 *  authors may be found in the AUTHORS file in the same directory.
 */

#ifndef BOMX_VP9_PARSER_H__
#define BOMX_VP9_PARSER_H__

void BOMX_Vp9_ParseSuperframe(const uint8_t *data, size_t data_sz, uint32_t sizes[8], unsigned *count);

#endif /* #ifndef BOMX_VP9_PARSER_H__ */
