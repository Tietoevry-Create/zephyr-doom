#pragma once

/* Minimal compiler abstraction used by legacy Doom sources.
 *
 * This project builds with GCC/Clang in Zephyr, so provide the `__ASM` macro
 * used for inline assembly (e.g., compiler memory barriers).
 */

#if defined(__clang__) || defined(__GNUC__)
#define __ASM __asm__
#else
#define __ASM asm
#endif
