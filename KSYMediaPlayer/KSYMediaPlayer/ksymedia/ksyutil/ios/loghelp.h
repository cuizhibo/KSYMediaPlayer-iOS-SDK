/*****************************************************************************
 * loghelper.h
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

#ifndef KSYUTIL_IOS__LOGHELP_H
#define KSYUTIL_IOS__LOGHELP_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KSY_LOG_TAG "KSYMEDIA"

#define KSY_LOG_UNKNOWN     0
#define KSY_LOG_DEFAULT     1

#define KSY_LOG_VERBOSE     2
#define KSY_LOG_DEBUG       3
#define KSY_LOG_INFO        4
#define KSY_LOG_WARN        5
#define KSY_LOG_ERROR       6
#define KSY_LOG_FATAL       7
#define KSY_LOG_SILENT      8

#define VLOG(level, TAG, ...)    ((void)vprintf(__VA_ARGS__))
#define VLOGV(...)  VLOG(KSY_LOG_VERBOSE,   KSY_LOG_TAG, __VA_ARGS__)
#define VLOGD(...)  VLOG(KSY_LOG_DEBUG,     KSY_LOG_TAG, __VA_ARGS__)
#define VLOGI(...)  VLOG(KSY_LOG_INFO,      KSY_LOG_TAG, __VA_ARGS__)
#define VLOGW(...)  VLOG(KSY_LOG_WARN,      KSY_LOG_TAG, __VA_ARGS__)
#define VLOGE(...)  VLOG(KSY_LOG_ERROR,     KSY_LOG_TAG, __VA_ARGS__)

#define ALOG(level, TAG, ...)    ((void)printf(__VA_ARGS__))
#define ALOGV(...)  ALOG(KSY_LOG_VERBOSE,   KSY_LOG_TAG, __VA_ARGS__)
#define ALOGD(...)  ALOG(KSY_LOG_DEBUG,     KSY_LOG_TAG, __VA_ARGS__)
#define ALOGI(...)  ALOG(KSY_LOG_INFO,      KSY_LOG_TAG, __VA_ARGS__)
#define ALOGW(...)  ALOG(KSY_LOG_WARN,      KSY_LOG_TAG, __VA_ARGS__)
#define ALOGE(...)  ALOG(KSY_LOG_ERROR,     KSY_LOG_TAG, __VA_ARGS__)
#define LOG_ALWAYS_FATAL(...)   do { ALOGE(__VA_ARGS__); exit(1); } while (0)

#ifdef __cplusplus
}
#endif

#endif
