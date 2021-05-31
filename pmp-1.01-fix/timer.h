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
 *  $Id: timer.h,v 1.3 2017/01/03 00:56:32 tanaka Exp tanaka $
 *  $Revision: 1.3 $
 *  $Date: 2017/01/03 00:56:32 $
 *  $Author: tanaka $
 *
 */
#ifndef TIMER_H
#define TIMER_H
#ifdef _WIN32
#define USE_CLOCK
#endif /* _WIN32 */
#include <stdio.h>
#ifdef USE_CLOCK
#include <time.h>
#else  /* !USE_CLOCK */
#include <sys/time.h>
#include <sys/resource.h>
#endif /* !USE_CLOCK */
#include "problem.h"

void timer_start(problem_t *);
void set_time(problem_t *);
double get_time(problem_t *);

#endif /* !TIMER_H */
