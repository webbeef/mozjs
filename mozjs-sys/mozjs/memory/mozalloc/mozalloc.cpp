/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=4 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stddef.h>  // for size_t

#if defined(MALLOC_H)
#  include MALLOC_H  // for memalign, malloc_size, malloc_us
#endif               // if defined(MALLOC_H)

#if !defined(MOZ_MEMORY)
// When jemalloc is disabled, route through the mozjs_sys bridge allocator
// symbols provided by the Rust consumer (or the default libc implementation).

#  include <stdlib.h>
#  include <string.h>
#  if defined(XP_UNIX)
#    include <unistd.h>
#  endif  // if defined(XP_UNIX)

#  include "mozjs_sys_alloc.h"

#  define malloc_impl mozjs_sys_malloc
#  define calloc_impl mozjs_sys_calloc
#  define realloc_impl mozjs_sys_realloc
#  define free_impl mozjs_sys_free
#  define memalign_impl mozjs_sys_memalign
#  define malloc_usable_size_impl mozjs_sys_malloc_usable_size

// strdup/strndup internally call system malloc. We provide versions
// that allocate through the bridge to keep alloc/free consistent.
static inline char* mozjs_sys_strdup_impl(const char* s) {
  if (!s) return nullptr;
  size_t len = strlen(s) + 1;
  char* d = static_cast<char*>(mozjs_sys_malloc(len));
  if (d) memcpy(d, s, len);
  return d;
}
static inline char* mozjs_sys_strndup_impl(const char* s, size_t n) {
  if (!s) return nullptr;
  size_t len = strnlen(s, n);
  char* d = static_cast<char*>(mozjs_sys_malloc(len + 1));
  if (d) {
    memcpy(d, s, len);
    d[len] = '\0';
  }
  return d;
}

#  define strdup_impl mozjs_sys_strdup_impl
#  define strndup_impl mozjs_sys_strndup_impl

#endif

#include <errno.h>
#include <new>  // for std::bad_alloc
#include <cstring>

#include <sys/types.h>

#include "mozilla/Assertions.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/Likely.h"
#include "mozilla/mozalloc.h"
#include "mozilla/mozalloc_oom.h"  // for mozalloc_handle_oom

#if defined(MOZ_MEMORY)
MOZ_MEMORY_API char* strdup_impl(const char*);
MOZ_MEMORY_API char* strndup_impl(const char*, size_t);
#endif

void* moz_xmalloc(size_t size) {
  void* ptr = malloc_impl(size);
  if (MOZ_UNLIKELY(!ptr && size)) {
    mozalloc_handle_oom(size);
    return moz_xmalloc(size);
  }
  return ptr;
}

void* moz_xcalloc(size_t nmemb, size_t size) {
  void* ptr = calloc_impl(nmemb, size);
  if (MOZ_UNLIKELY(!ptr && nmemb && size)) {
    mozilla::CheckedInt<size_t> totalSize =
        mozilla::CheckedInt<size_t>(nmemb) * size;
    mozalloc_handle_oom(totalSize.isValid() ? totalSize.value() : SIZE_MAX);
    return moz_xcalloc(nmemb, size);
  }
  return ptr;
}

void* moz_xrealloc(void* ptr, size_t size) {
  void* newptr = realloc_impl(ptr, size);
  if (MOZ_UNLIKELY(!newptr && size)) {
    mozalloc_handle_oom(size);
    return moz_xrealloc(ptr, size);
  }
  return newptr;
}

char* moz_xstrdup(const char* str) {
  char* dup = strdup_impl(str);
  if (MOZ_UNLIKELY(!dup)) {
    mozalloc_handle_oom(0);
    return moz_xstrdup(str);
  }
  return dup;
}

#if defined(HAVE_STRNDUP)
char* moz_xstrndup(const char* str, size_t strsize) {
  char* dup = strndup_impl(str, strsize);
  if (MOZ_UNLIKELY(!dup)) {
    mozalloc_handle_oom(strsize);
    return moz_xstrndup(str, strsize);
  }
  return dup;
}
#endif  // if defined(HAVE_STRNDUP)

void* moz_xmemdup(const void* ptr, size_t size) {
  void* newPtr = moz_xmalloc(size);
  memcpy(newPtr, ptr, size);
  return newPtr;
}

#ifndef __wasm__
#  ifndef HAVE_MEMALIGN
// We always have a definition of memalign, but system headers don't
// necessarily come with a declaration.
extern "C" void* memalign(size_t, size_t);
#  endif

void* moz_xmemalign(size_t boundary, size_t size) {
  void* ptr = memalign_impl(boundary, size);
  if (MOZ_UNLIKELY(!ptr && EINVAL != errno)) {
    mozalloc_handle_oom(size);
    return moz_xmemalign(boundary, size);
  }
  // non-NULL ptr or errno == EINVAL
  return ptr;
}
#endif

size_t moz_malloc_usable_size(void* ptr) {
  if (!ptr) return 0;

#if defined(MOZ_MEMORY)
#  if defined(XP_DARWIN)
  return malloc_size(ptr);
#  elif defined(HAVE_MALLOC_USABLE_SIZE) || defined(MOZ_MEMORY)
  return malloc_usable_size_impl(ptr);
#  elif defined(XP_WIN)
  return _msize(ptr);
#  else
  return 0;
#  endif
#else
  return mozjs_sys_malloc_usable_size(ptr);
#endif
}

size_t moz_malloc_size_of(const void* ptr) {
  return moz_malloc_usable_size((void*)ptr);
}

#if defined(MOZ_MEMORY)
#  include "mozjemalloc_types.h"
// mozmemory.h declares jemalloc_ptr_info(), but including that header in this
// file is complicated. So we just redeclare it here instead, and include
// mozjemalloc_types.h for jemalloc_ptr_info_t.
MOZ_JEMALLOC_API void jemalloc_ptr_info(const void* ptr,
                                        jemalloc_ptr_info_t* info);
#endif

size_t moz_malloc_enclosing_size_of(const void* ptr) {
#if defined(MOZ_MEMORY)
  jemalloc_ptr_info_t info;
  jemalloc_ptr_info(ptr, &info);
  return jemalloc_ptr_is_live(&info) ? info.size : 0;
#else
  return 0;
#endif
}
