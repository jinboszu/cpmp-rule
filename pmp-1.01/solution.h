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
 *  $Id: solution.h,v 1.6 2017/01/03 00:56:32 tanaka Exp tanaka $
 *  $Revision: 1.6 $
 *  $Date: 2017/01/03 00:56:32 $
 *  $Author: tanaka $
 *
 */
#ifndef SOLUTION_H
#define SOLUTION_H
#include "define.h"
#include "problem.h"

typedef struct {
  int src;
  int dst;
  block_t block;
} relocation_t;

typedef struct {
  int n_relocation;
  int n_block;
  relocation_t *relocation;
} solution_t;

typedef struct {
  int n_tier;
  int n_clean;
  int clean_priority;
  int misoverlay_priority;
  uchar upside_down;
  int last_change;
} stack_state_t;

typedef struct {
  int misoverlay_priority;
  uchar upside_down;
} block_state_t;

typedef struct {
  int n_misoverlay;

  stack_state_t *stack;
  block_t **block;
  block_state_t **block_state;

  /* history */
  int *last_relocation;
} state_t;

typedef struct {
  int lb;
  int lbBX;
  int lbGX;
  int n_dirty_stack;
  int n_full_clean_stack;

  int *demand;
  int *supply;
  int **removal_for_supply;
} lb_state_t;


solution_t *create_solution(void);
void free_solution(solution_t *);
void copy_solution(solution_t *, solution_t *);
void add_relocation(solution_t *, int, int, block_t *);
state_t *create_state(problem_t *);
lb_state_t *lb_create_state(problem_t *);
state_t *initialize_state(problem_t *, state_t *);
lb_state_t *initialize_lb_state(problem_t *, state_t *, lb_state_t *);
void copy_state(problem_t *, state_t *, state_t *);
void copy_lb_state(problem_t *, lb_state_t *, lb_state_t *);
state_t *duplicate_state(problem_t *, state_t *);
lb_state_t *duplicate_lb_state(problem_t *, lb_state_t *);
void free_state(state_t *);
void free_lb_state(lb_state_t *);
void update_state(problem_t *, state_t *, int, int);
uchar update_state_src(problem_t *, state_t *, lb_state_t *, int, int);
uchar update_state_dst(problem_t *, state_t *, lb_state_t *, block_t *,
                       int, int);

#endif /* !SOLUTION_H */
