/**
 *  Copyright (C) 2011-2012  Juho Vähä-Herttua
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *=================================================================
 * modified by fduncanh 2022
 */

#ifndef THREADS_H
#define THREADS_H

/* Always use pthread library */

#include <pthread.h>
#ifndef _WIN32
#include <unistd.h>
#endif

#define sleepms(x) usleep((x)*1000)

typedef pthread_t thread_handle_t;

#define THREAD_RETVAL void *
#ifndef _MSC_VER
#define THREAD_CREATE(handle, func, arg) \
	if (pthread_create(&(handle), NULL, func, arg)) handle = 0
#else
#define THREAD_CREATE(handle, func, arg) \
	pthread_create(&(handle), NULL, func, arg)
#endif
#define THREAD_JOIN(handle) pthread_join(handle, NULL)

typedef pthread_mutex_t mutex_handle_t;

typedef pthread_cond_t cond_handle_t;

#define MUTEX_CREATE(handle) pthread_mutex_init(&(handle), NULL)
#define MUTEX_LOCK(handle) pthread_mutex_lock(&(handle))
#define MUTEX_UNLOCK(handle) pthread_mutex_unlock(&(handle))
#define MUTEX_DESTROY(handle) pthread_mutex_destroy(&(handle))

#define COND_CREATE(handle) pthread_cond_init(&(handle), NULL)
#define COND_SIGNAL(handle) pthread_cond_signal(&(handle))
#define COND_DESTROY(handle) pthread_cond_destroy(&(handle))

#endif /* THREADS_H */
