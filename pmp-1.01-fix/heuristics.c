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
 *  $Id: heuristics.c,v 1.20 2017/03/24 11:08:49 tanaka Exp tanaka $
 *  $Revision: 1.20 $
 *  $Date: 2017/03/24 11:08:49 $
 *  $Author: tanaka $
 *
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "define.h"
#include "heuristics.h"
#include "print.h"
#include "problem.h"
#include "solution.h"

uchar heuristics(problem_t *problem, state_t *state, solution_t *solution,
                 int upper_bound)
{
  int i, j;
  block_t block;
  int n_clean_stack = 0, n_dirty_stack = 0;
  stack_state_t *stack;
  solution_t *csolution = solution;
  static state_t *cstate = NULL;
  static int *clean_stack = NULL, *dirty_stack = NULL;

  if(problem == NULL) {
    if(cstate != NULL) {
      free_state(cstate);
      cstate = NULL;
    }
    if(clean_stack != NULL) {
      free(clean_stack);
      clean_stack = NULL;
    }
    return(False);
  }

#if 0
  printf("heuristics\n");
#endif

  if(clean_stack == NULL) {
    clean_stack = (int *) malloc((size_t) 2*problem->n_stack*sizeof(int));
    dirty_stack = clean_stack + problem->n_stack;
  }

  if(csolution == NULL) {
    csolution = create_solution();
  }

  stack = state->stack;
  for(i = 0; i < problem->n_stack; ++i) {
    if(stack[i].n_clean == stack[i].n_tier) {
      if(stack[i].n_tier < problem->s_height) {
        for(j = n_clean_stack; j > 0
              && stack[clean_stack[j - 1]].clean_priority
              < stack[i].clean_priority; --j) {
          clean_stack[j] = clean_stack[j - 1];
        }
        clean_stack[j] = i;
        ++n_clean_stack;
      }
    } else {
      dirty_stack[n_dirty_stack++] = i;
    }
  }

  if(n_clean_stack == 0) {
    csolution->n_relocation = MAX_N_RELOCATION + 1;
    return(False);
  }

  if(cstate == NULL) {
    cstate = duplicate_state(problem, state);
  } else {
    copy_state(problem, cstate, state);
  }

  stack = cstate->stack;
  while(csolution->n_relocation < upper_bound) {
    int src_index = -1, dst_index = -1;
    int src_stack = -1, dst_stack;
    int decrease, min_decrease = problem->max_priority + 1;
    int priority;

    /* BG relocation */
    for(i = 0; i < n_dirty_stack; ++i) {
      src_stack = dirty_stack[i];
      priority
        = cstate->block[src_stack][stack[src_stack].n_tier - 1].priority;

      /* BG relocation does not exist */
      if(stack[clean_stack[0]].clean_priority < priority) {
        continue;
      }

      /* destionation stack has the maximum priority among candidates */
      for(j = n_clean_stack - 1; j >= 0; --j) {
        if(stack[clean_stack[j]].clean_priority >= priority) {
          break;
        }
      }

      /* decrease of destination stack priority */
      decrease = stack[clean_stack[j]].clean_priority - priority;

      /* heuristic rule for next relocation */
#if 1
      if(decrease < min_decrease
         || (decrease == min_decrease
             && stack[src_stack].clean_priority
             > stack[dirty_stack[src_index]].clean_priority)
         || (decrease == min_decrease
             && stack[src_stack].clean_priority
             == stack[dirty_stack[src_index]].clean_priority
             && stack[src_stack].n_tier - stack[src_stack].n_clean 
             > stack[dirty_stack[src_index]].n_tier
             - stack[dirty_stack[src_index]].n_clean)) {
#else
      if(decrease < min_decrease
         || (decrease == min_decrease
             && stack[src_stack].misoverlay_priority
             > stack[dirty_stack[src_index]].misoverlay_priority)
         || (decrease == min_decrease
             && stack[src_stack].misoverlay_priority
             == stack[dirty_stack[src_index]].misoverlay_priority
             && stack[src_stack].clean_priority
             > stack[dirty_stack[src_index]].clean_priority)) {
#endif
        min_decrease = decrease;
        src_index = i;
        dst_index = j;
#if 1
      }
#else
      }
#endif
    }

    if(dst_index >= 0) {
      src_stack = dirty_stack[src_index];
    } else {
      /* GG relocation */
      int increase, max_increase = 1;
      int max_priority = -1;
      int last_dst = -1;

      /* destination stack of last relocation */
      if(csolution->n_relocation > 0) {
        last_dst = csolution->relocation[csolution->n_relocation - 1].dst;
      }

      for(i = 0; i < problem->n_stack; ++i) {
        if(i == last_dst || stack[i].n_clean < stack[i].n_tier
           || stack[i].clean_priority > stack[clean_stack[0]].clean_priority) {
          continue;
        }

        if(stack[i].n_tier == 1) {
          priority = problem->max_priority;
        } else {
          priority = cstate->block[i][stack[i].n_tier - 2].priority;
        }

        /* increase of source stack priority */
        increase = priority - stack[i].clean_priority;

        if(increase == 0) {
          continue;
        }

        /* minimize decrease of destination stack priority */
        for(j = n_clean_stack - 1; j >= 0; --j) {
          if(clean_stack[j] != i
             && stack[clean_stack[j]].clean_priority
             >= stack[i].clean_priority) {
            break;
          }
        }
        if(j < 0) {
          continue;
        }

        increase
          -= stack[clean_stack[j]].clean_priority - stack[i].clean_priority;

        /* heuristic rule for next relocation */
        if(increase > max_increase
           || (increase == max_increase && priority > max_priority)
           || (increase == max_increase && priority == max_priority
               && stack[i].n_tier < stack[src_stack].n_tier)) {
          max_increase = increase;
          max_priority = priority;
          src_stack = i;
          dst_index = j;
        }
      }

      /* no BG or GG relocation is found */
      if(dst_index < 0) {
        break;
      }
    }

    dst_stack = clean_stack[dst_index];
    block = cstate->block[src_stack][stack[src_stack].n_tier - 1];
    update_state(problem, cstate, src_stack, dst_stack);
    add_relocation(csolution, src_stack, dst_stack, &block);

    /* solved */
    if(cstate->n_misoverlay == 0) {
      break;
    }

    /* update clean_stack and dirty_stack */
    if(stack[dst_stack].n_tier == problem->s_height
       || stack[dst_stack].n_clean < stack[dst_stack].n_tier) {
      for(i = dst_index; i < n_clean_stack - 1; ++i) {
        clean_stack[i] = clean_stack[i + 1];
      }
      --n_clean_stack;

      if(stack[dst_stack].n_clean == stack[dst_stack].n_tier - 1) {
        dirty_stack[n_dirty_stack++] = dst_stack;
      }
    }

    if(stack[src_stack].n_clean == stack[src_stack].n_tier) {
      if(stack[src_stack].n_tier == problem->s_height - 1
         || block.priority > stack[src_stack].clean_priority) {
        for(i = n_clean_stack; i > 0
              && stack[clean_stack[i - 1]].clean_priority
              < stack[src_stack].clean_priority; --i) {
          clean_stack[i] = clean_stack[i - 1];
        }
        clean_stack[i] = src_stack;
        ++n_clean_stack;

        if(block.priority > stack[src_stack].clean_priority) {
          if(src_index >= 0) {
            i = src_index;
          } else {
            for(i = 0; dirty_stack[i] != src_stack; ++i);
          }

          for(; i < n_dirty_stack - 1; ++i) {
            dirty_stack[i] = dirty_stack[i + 1];
          }
          --n_dirty_stack;
        }
      } else {
        for(i = 0; clean_stack[i] != src_stack; ++i);
        for(; i > 0
              && stack[clean_stack[i - 1]].clean_priority
              < stack[src_stack].clean_priority; --i) {
          clean_stack[i] = clean_stack[i - 1];
        }
        clean_stack[i] = src_stack;
      }
    }

    if(n_clean_stack == 0) {
      csolution->n_relocation = MAX_N_RELOCATION + 1;
      return(False);
    }
  }

  if(cstate->n_misoverlay == 0 && csolution->n_relocation < upper_bound) {
    return(True);
  }

  csolution->n_relocation = MAX_N_RELOCATION + 1;
  return(False);
}
