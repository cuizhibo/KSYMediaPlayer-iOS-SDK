/*****************************************************************************
 * ksyutil.h
 *****************************************************************************
 *
 * This file is part of ksyPlayer.
 *
 * ksyPlayer is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * ksyPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with ksyPlayer; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef KSYUTIL__KSYUTIL_H
#define KSYUTIL__KSYUTIL_H

#include <stdlib.h>
#include <memory.h>
#include "ksylog.h"

#ifndef KSYMAX
#define KSYMAX(a, b)    ((a) > (b) ? (a) : (b))
#endif

#ifndef KSYMIN
#define KSYMIN(a, b)    ((a) < (b) ? (a) : (b))
#endif

#ifndef KSYALIGN
#define KSYALIGN(x, align) ((( x ) + (align) - 1) / (align) * (align))
#endif

#define KSY_CHECK_RET(condition__, retval__, ...) \
    if (!(condition__)) { \
        ALOGE(__VA_ARGS__); \
        return (retval__); \
    }

inline static void *mallocz(size_t size)
{
    void *mem = malloc(size);
    if (!mem)
        return mem;

    memset(mem, 0, size);
    return mem;
}

inline static void freep(void **mem)
{
    if (mem && *mem) {
        free(*mem);
        *mem = NULL;
    }
}

#if defined(__ANDROID__)
#include "android/ksyutil_android.h"
#elif defined(__APPLE__)
#include "ios/ksyutil_ios.h"
#endif

#endif
