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
 *  $Id: print.c,v 1.8 2017/02/15 10:05:56 tanaka Exp tanaka $
 *  $Revision: 1.8 $
 *  $Date: 2017/02/15 10:05:56 $
 *  $Author: tanaka $
 *
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "define.h"
#include "print.h"
#include "problem.h"
#include "solution.h"

void print_problem(problem_t *problem, FILE *fp)
{
  int i, j;
  int max_tier;

  fprintf(fp, "stacks=%d, s_height=%d, blocks=%d\n", problem->n_stack, 
          problem->s_height, problem->n_block);

  max_tier = 0;
  for(i = 0; i < problem->n_stack; ++i) {
    max_tier = max(max_tier, problem->n_tier[i]);
  }

  for(j = max_tier - 1; j >= 0; --j) {
    fprintf(fp, "%3d:", j + 1);
    for(i = 0; i < problem->n_stack; ++i) {
      if(j < problem->n_tier[i]) {
        fprintf(fp, "[%2d]", problem->priority[problem->block[i][j].no]);
      } else {
        fprintf(fp, "    ");
      }
    }
    fprintf(fp, "\n");
  }

  fprintf(fp, "   ");
  for(i = 0; i < problem->n_stack; ++i) {
    fprintf(fp, "----");
  }
  fprintf(fp, "\n");

  fprintf(fp, "   ");
  for(i = 0; i < problem->n_stack; ++i) {
    fprintf(fp, " %2d ", i + 1);
  }
  fprintf(fp, "\n");
}

void print_state(problem_t *problem, state_t *state, FILE *fp)
{
  int i, j;
  int max_tier = 0;

  for(i = 0; i < problem->n_stack; ++i) {
    max_tier = max(max_tier, state->stack[i].n_tier);
  }
  for(j = max_tier - 1; j >= 0; --j) {
    fprintf(fp, "%2d:", j + 1);
    for(i = 0; i < problem->n_stack; ++i) {
      if(j < state->stack[i].n_clean) {
        fprintf(fp, "[%2d]", problem->priority[state->block[i][j].no]);
      } else if(j < state->stack[i].n_tier) {
        fprintf(fp, "<%2d>", problem->priority[state->block[i][j].no]);
      } else {
        fprintf(fp, "    ");
      }
    }
    fprintf(fp, "\n");
  }

  fprintf(fp, "   ");
  for(i = 0; i < problem->n_stack; ++i) {
    fprintf(fp, "----");
  }
  fprintf(fp, "\n");

  fprintf(fp, "   ");
  for(i = 0; i < problem->n_stack; ++i) {
    fprintf(fp, " %2d ", i + 1);
  }
  fprintf(fp, "\n");
}

void print_solution(problem_t *problem, solution_t *solution, FILE *fp)
{
  int i;
  int src_stack, dst_stack;
  block_t reloc_block;
  state_t *state = initialize_state(problem, NULL);
  stack_state_t *stack = state->stack;

  fprintf(fp, "========\nInitial configuration\n");
  print_state(problem, state, fp);

  for(i = 0; i < solution->n_relocation; ++i) {
    src_stack = solution->relocation[i].src;
    dst_stack = solution->relocation[i].dst;
    reloc_block = state->block[src_stack][stack[src_stack].n_tier - 1];

    if(solution->relocation[i].block.no != reloc_block.no) {
      fprintf(stderr, "Item mismatch in solution %d!=%d.\n,",
              solution->relocation[i].block.no, reloc_block.no);
      exit(1);
    }

    if(stack[dst_stack].n_tier == problem->s_height) {
      fprintf(stderr, "No space left in stack %d\n", dst_stack);
      exit(1);
    }

    fprintf(fp, "--------\n");
    if(stack[src_stack].n_clean < stack[src_stack].n_tier) {
      fprintf(fp, "Relocation %d: <%2d> %d->%d\n", i + 1,
              problem->priority[reloc_block.no], src_stack + 1, dst_stack + 1);
    } else {
      fprintf(fp, "Relocation %d: [%2d] %d->%d\n", i + 1,
              problem->priority[reloc_block.no], src_stack + 1, dst_stack + 1);
    }

    update_state(problem, state, src_stack, dst_stack);

    print_state(problem, state, fp);
  }
  fprintf(fp, "--------\n");

  fprintf(fp, "relocations=%d\n", solution->n_relocation);

  if(state->n_misoverlay != 0) {
    fprintf(stderr, "Invalid solution. badly placed blocks=%d\n",
            state->n_misoverlay);
    exit(1);
  }

  free_state(state);
}

void print_solution_relocation(problem_t *problem, solution_t *solution,
                               FILE *fp)
{
  int i;

  for(i = 0; i < solution->n_relocation; ++i) {
    fprintf(fp, "[%d=>%d(%d)]", solution->relocation[i].src + 1,
            solution->relocation[i].dst + 1,
            problem->priority[solution->relocation[i].block.no]);
  }
  if(solution->n_relocation > 0) {
    fprintf(fp, "\n");
  }
}

void print_time(problem_t *problem)
{
  set_time(problem);
  fprintf(stderr, "time=%.2f\n", problem->time);
}
