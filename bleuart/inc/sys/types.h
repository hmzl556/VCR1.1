/*
 * Minimal sys/types.h shim for bare-metal toolchains (Keil/ARMCC).
 *
 * This project includes third-party SBC sources originally written for
 * POSIX environments. They only rely on sys/types.h for ssize_t/size_t.
 * Provide the missing definitions here to keep the upstream sources intact.
 */

#ifndef BR1601_SYS_TYPES_H
#define BR1601_SYS_TYPES_H

#include <stddef.h>   /* size_t */
#include <stdint.h>   /* int32_t, etc. */

#ifdef __cplusplus
extern "C" {
#endif

/* Signed counterpart of size_t (POSIX). */
typedef long ssize_t;

#ifdef __cplusplus
}
#endif

#endif /* BR1601_SYS_TYPES_H */
