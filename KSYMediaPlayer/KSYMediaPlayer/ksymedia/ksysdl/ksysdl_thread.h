/*****************************************************************************
 * ksysdl_thread.h
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

#ifndef KSYSDL__KSYSDL_THREAD_H
#define KSYSDL__KSYSDL_THREAD_H

#include <stdint.h>
#include <pthread.h>

typedef enum {
    SDL_THREAD_PRIORITY_LOW,
    SDL_THREAD_PRIORITY_NORMAL,
    SDL_THREAD_PRIORITY_HIGH
} SDL_ThreadPriority;

typedef struct SDL_Thread
{
    pthread_t id;
    int (*func)(void *);
    void *data;
    char name[32];
    int retval;
} SDL_Thread;

SDL_Thread *SDL_CreateThreadEx(SDL_Thread *thread, int (*fn)(void *), void *data, const char *name);
int         SDL_SetThreadPriority(SDL_ThreadPriority priority);
void        SDL_WaitThread(SDL_Thread *thread, int *status);
void        SDL_WaitThread_kill(SDL_Thread *thread, int *status);

#endif
