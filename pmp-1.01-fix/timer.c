/*
 * Copyright 2016-2017 Shunji Tanaka and Kevin Tierney.  All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials
 *      provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  $Id: timer.c,v 1.3 2017/01/03 00:56:32 tanaka Exp tanaka $
 *  $Revision: 1.3 $
 *  $Date: 2017/01/03 00:56:32 $
 *  $Author: tanaka $
 *
 */
#include <stdio.h>
#include "define.h"
#include "timer.h"
#include "problem.h"
#define INCLUDE_SYSTEM_TIME

void timer_start(problem_t *problem)
{
#ifdef USE_CLOCK
  problem->stime = (double) clock()/(double) CLOCKS_PER_SEC;
#else  /* !USE_CLOCK */
  struct rusage ru;

  getrusage(RUSAGE_SELF, &ru);
  problem->stime = ru.ru_utime.tv_sec + ru.ru_utime.tv_usec / 1000000.0;
#ifdef INCLUDE_SYSTEM_TIME
  problem->stime += ru.ru_stime.tv_sec + ru.ru_stime.tv_usec / 1000000.0;
#endif /* INCLUDE_SYSTEM_TIME */
#endif /* USE_CLOCK */
}

void set_time(problem_t *problem)
{
#ifdef USE_CLOCK
  problem->time = (double) clock()/(double) CLOCKS_PER_SEC;
#else  /* !USE_CLOCK */
  struct rusage ru;

  getrusage(RUSAGE_SELF, &ru);
  problem->time = ru.ru_utime.tv_sec + ru.ru_utime.tv_usec / 1000000.0;
#ifdef INCLUDE_SYSTEM_TIME
  problem->time += ru.ru_stime.tv_sec + ru.ru_stime.tv_usec / 1000000.0;
#endif /* INCLUDE_SYSTEM_TIME */
#endif /* !USE_CLOCK */
  problem->time -= problem->stime;

  if(problem->time < 0.0) {
    problem->time = 0.0;
  }
}

double get_time(problem_t *problem)
{
  double t;
#ifdef USE_CLOCK
  t = (double) clock()/(double) CLOCKS_PER_SEC;
#else  /* !USE_CLOCK */
  struct rusage ru;

  getrusage(RUSAGE_SELF, &ru);
  t = ru.ru_utime.tv_sec + ru.ru_utime.tv_usec / 1000000.0;
#ifdef INCLUDE_SYSTEM_TIME
  t += ru.ru_stime.tv_sec + ru.ru_stime.tv_usec / 1000000.0;
#endif /* INCLUDE_SYSTEM_TIME */
#endif /* !USE_CLOCK */
  t -= problem->stime;

  return((t < 0.0)?0.0:t);
}
