/*
 * Minimal errno.h shim for bare-metal toolchains (Keil/ARMCC).
 *
 * Some third-party sources (e.g. SBC) expect common POSIX errno macros.
 * If the toolchain libc does not provide them, define a small subset.
 *
 * NOTE: Values are chosen to be stable within this project. They only need
 * to be unique and non-zero for error reporting in this codebase.
 */

#ifndef BR1601_ERRNO_H
#define BR1601_ERRNO_H

/* If the toolchain already provides these, keep its values. */
#ifndef EIO
#define EIO 5
#endif

#ifndef ENOMEM
#define ENOMEM 6
#endif

#ifndef EINVAL
#define EINVAL 22
#endif

#ifndef ENOSPC
#define ENOSPC 28
#endif

#endif /* BR1601_ERRNO_H */
