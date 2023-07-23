/* =============================================================================
 *
 * thread.h
 *
 * =============================================================================
 *
 * Copyright (C) Stanford University, 2006.  All Rights Reserved.
 * Author: Chi Cao Minh
 *
 * =============================================================================
 *
 * For the license of bayes/sort.h and bayes/sort.c, please see the header
 * of the files.
 *
 * ------------------------------------------------------------------------
 *
 * For the license of kmeans, please see kmeans/LICENSE.kmeans
 *
 * ------------------------------------------------------------------------
 *
 * For the license of ssca2, please see ssca2/COPYRIGHT
 *
 * ------------------------------------------------------------------------
 *
 * For the license of lib/mt19937ar.c and lib/mt19937ar.h, please see the
 * header of the files.
 *
 * ------------------------------------------------------------------------
 *
 * For the license of lib/rbtree.h and lib/rbtree.c, please see
 * lib/LEGALNOTICE.rbtree and lib/LICENSE.rbtree
 *
 * ------------------------------------------------------------------------
 *
 * Unless otherwise noted, the following license applies to STAMP files:
 *
 * Copyright (c) 2007, Stanford University
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *
 *     * Neither the name of Stanford University nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY STANFORD UNIVERSITY ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL STANFORD UNIVERSITY BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * =============================================================================
 */

#pragma once

#include <assert.h>
#include <pthread.h>
#include <stdlib.h>

static __thread long file_threadId;
static long file_numThread = 1;
static pthread_barrier_t* file_barrierPtr = NULL;
static long* file_threadIds = NULL;
static pthread_t* file_threads = NULL;
static void (*file_funcPtr)(void*) = NULL;
static void* file_argPtr = NULL;
static volatile bool file_doShutdown = false;

/**
 * threadWait: Synchronizes all threads to start/stop parallel section
 */
template <typename S>
static void* threadWait(void* argPtr) {
  S::thread_init();
  long threadId = *(long*)argPtr;

  file_threadId = (long)threadId;

  while (1) {
    pthread_barrier_wait(file_barrierPtr); /* wait for start parallel */
    if (file_doShutdown) {
      break;
    }
    file_funcPtr(file_argPtr);
    pthread_barrier_wait(file_barrierPtr); /* wait for end parallel */
    if (threadId == 0) {
      break;
    }
  }
  return NULL;
}

/* =============================================================================
 * thread_startup
 * -- Create pool of secondary threads
 * -- numThread is total number of threads (primary + secondary)
 * =============================================================================
 */
template <typename S>
void thread_startup(long numThread, size_t numPool) {
  S::global_init(numThread, numPool);

  file_numThread = numThread;
  file_doShutdown = false;

  // Set up barrier
  assert(file_barrierPtr == NULL);
  file_barrierPtr = (pthread_barrier_t*)malloc(sizeof(pthread_barrier_t));
  assert(file_barrierPtr);
  pthread_barrier_init(file_barrierPtr, 0, numThread);

  // Set up ids
  assert(file_threadIds == NULL);
  file_threadIds = (long*)malloc(numThread * sizeof(long));
  assert(file_threadIds);
  for (long i = 0; i < numThread; i++) {
    file_threadIds[i] = i;
  }

  // Set up thread list
  assert(file_threads == NULL);
  file_threads = (pthread_t*)malloc(numThread * sizeof(pthread_t));
  assert(file_threads);

  // Set up pool
  for (long i = 1; i < numThread; i++) {
    pthread_create(&file_threads[i], NULL, &threadWait<S>,
                   (void*)&file_threadIds[i]);
  }

  // Wait for primary thread to call thread_start
}

/* =============================================================================
 * thread_start
 * -- Make primary and secondary threads execute work
 * -- Should only be called by primary thread
 * -- funcPtr takes one arguments: argPtr
 * =============================================================================
 */
template <typename S>
void thread_start(void (*funcPtr)(void*), void* argPtr) {
  file_funcPtr = funcPtr;
  file_argPtr = argPtr;

  long threadId = 0; /* primary */
  threadWait<S>((void*)&threadId);
}

/* =============================================================================
 * thread_shutdown
 * -- Primary thread kills pool of secondary threads
 * =============================================================================
 */
template <typename S>
void thread_shutdown() {
  // Make secondary threads exit wait()
  file_doShutdown = true;
  pthread_barrier_wait(file_barrierPtr);

  long numThread = file_numThread;

  for (long i = 1; i < numThread; i++) {
    pthread_join(file_threads[i], NULL);
  }

  free(file_barrierPtr);
  file_barrierPtr = NULL;

  free(file_threadIds);
  file_threadIds = NULL;

  free(file_threads);
  file_threads = NULL;

  file_numThread = 1;
}

/* =============================================================================
 * thread_getId
 * -- Call after thread_start() to get thread ID inside parallel region
 * =============================================================================
 */
template <typename S>
long thread_getId() {
  return file_threadId;
}

/* =============================================================================
 * thread_getNumThread
 * -- Call after thread_start() to get number of threads inside parallel region
 * =============================================================================
 */
template <typename S>
long thread_getNumThread() {
  return file_numThread;
}

/* =============================================================================
 * thread_barrier_wait
 * -- Call after thread_start() to synchronize threads inside parallel region
 * =============================================================================
 */
template <typename S>
void thread_barrier_wait() {
  pthread_barrier_wait(file_barrierPtr);
}
