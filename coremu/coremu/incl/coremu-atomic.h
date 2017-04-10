/*
 * COREMU Parallel Emulator Framework
 *
 * Atomic support for COREMU system.
 * XXX: Now only support x86-64 architecture.
 *
 * Copyright (C) 2010 Parallel Processing Institute (PPI), Fudan Univ.
 *  <http://ppi.fudan.edu.cn/system_research_group>
 *
 * Authors:
 *  Zhaoguo Wang    <zgwang@fudan.edu.cn>
 *  Yufei Chen      <chenyufei@fudan.edu.cn>
 *  Ran Liu         <naruilone@gmail.com>
 *  Xi Wu           <wuxi@fudan.edu.cn>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _COREMU_ATOMIC_H
#define _COREMU_ATOMIC_H

#include <stdint.h>

#define __inline__ inline __attribute__((always_inline))

static __inline__ uint8_t bit_testandset(int *base, int off)
{
    uint8_t readval = 0;

    /* CF <-- Bit(BitBase, BitOffset). */
    __asm__ __volatile__ (
           "lock; btsl %2,%0\n\t"
            "setb %1\n\t"
            : "=m" (*base),"=a" (readval)
            : "Ir" (off)
            : "cc");

    return readval;
}

static __inline__ uint8_t bit_testandreset(int *base, int off)
{
    uint8_t readval = 0;

    /* CF <-- Bit(BitBase, BitOffset). */
    __asm__ __volatile__ (
            "lock; btrl %2,%0\n\t"
            "setb %1\n\t"
            : "=m" (*base),"=a" (readval)
            : "Ir" (off)
            : "cc");

    return readval;
}

static __inline__ uint8_t bit_test(int *base, int off)
{
    uint8_t readval = 0;

    /* CF <-- Bit(BitBase, BitOffset). */
    __asm__ __volatile__ (
            "lock; bt %2,%0\n\t"
            "setb %1\n\t"
            : "=m" (*base),"=a" (readval)
            : "Ir" (off)
            : "cc");

    return readval;
}

// Is this the correct way to detect 64 system?
#if (__LP64__== 1)
static __inline__ uint8_t
atomic_compare_exchange16b(uint64_t *memp,
                           uint64_t rax, uint64_t rdx,
                           uint64_t rbx, uint64_t rcx)
{
    uint8_t z;
    __asm __volatile__ ( "lock; cmpxchg16b %3\n\t"
                         "setz %2\n\t"
                         : "=a" (rax), "=d" (rdx), "=r" (z), "+m" (*memp)
                         : "a" (rax), "d" (rdx), "b" (rbx), "c" (rcx)
                         : "memory", "cc" );
    return z;
}
#endif

/* Memory Barriers: x86-64 ONLY now */
#define mb()    asm volatile("mfence":::"memory")
#define rmb()   asm volatile("lfence":::"memory")
#define wmb()   asm volatile("sfence" ::: "memory")

#define LOCK_PREFIX "lock; "

#define coremu_xglue(a, b) a ## b
// If a/b is macro, it will expand first, then pass to coremu_xglue
#define coremu_glue(a, b) coremu_xglue(a, b)

#define coremu_xstr(s) # s
#define coremu_str(s) coremu_xstr(s)

#define DATA_BITS 8
#include "atomic-template.h"

#define DATA_BITS 16
#include "atomic-template.h"

#define DATA_BITS 32
#include "atomic-template.h"

#if (__LP64__== 1)
#define DATA_BITS 64
#include "atomic-template.h"
#endif

static inline uint64_t atomic_xadd2(uint64_t *target)
{
    register uint64_t __result;
    asm volatile (
        "mov $2, %0\n"
        "lock xaddq %0, %1\n"
        : "=r" (__result), "+m" (*target)
        :
        : "cc", "memory"
    );
    return __result;
}

#endif /* _COREMU_ATOMIC_H */

