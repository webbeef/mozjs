/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Bridge allocator symbols provided by the Rust crate consumer.
 * When the mozjs-sys `custom-allocator` feature is disabled, default
 * implementations forwarding to libc are provided.  When enabled, the
 * consumer must supply #[no_mangle] extern "C" definitions for every
 * symbol declared here. */

#ifndef mozjs_sys_alloc_h
#define mozjs_sys_alloc_h

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void* mozjs_sys_malloc(size_t size);
void* mozjs_sys_calloc(size_t count, size_t size);
void* mozjs_sys_realloc(void* ptr, size_t size);
void mozjs_sys_free(void* ptr);
void* mozjs_sys_memalign(size_t alignment, size_t size);
size_t mozjs_sys_malloc_usable_size(const void* ptr);

#ifdef __cplusplus
}
#endif

#endif /* mozjs_sys_alloc_h */
