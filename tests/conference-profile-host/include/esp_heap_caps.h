#pragma once
#include <cstdlib>
static constexpr int MALLOC_CAP_SPIRAM=1,MALLOC_CAP_8BIT=2;
static inline void *heap_caps_malloc(size_t n,int){return malloc(n);}
static inline void *heap_caps_realloc(void *p,size_t n,int){return realloc(p,n);}
static inline void heap_caps_free(void *p){free(p);}
