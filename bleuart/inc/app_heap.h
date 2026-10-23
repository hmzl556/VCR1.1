#ifndef APP_HEAP_H
#define APP_HEAP_H
#include <stdlib.h>
#ifdef __CC_ARM
#include "cs_core.h"
/* Share the SDK's IRQ-protected C heap; a plain malloc/free in task
 * context can otherwise be interrupted by the SDK allocator. */
static void *app_heap_alloc(size_t n){return brt_malloc((uint32_t)n);}
static void app_heap_free(void *p){if(p)brt_free(p);}
#else
/* Host fault-injection tests retain their allocator hooks. */
static void *app_heap_alloc(size_t n){return malloc(n);}
static void app_heap_free(void *p){free(p);}
#endif
#endif
