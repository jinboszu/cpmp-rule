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
 *  $Id: solution.c,v 1.10 2017/02/28 08:43:47 tanaka Exp tanaka $
 *  $Revision: 1.10 $
 *  $Date: 2017/02/28 08:43:47 $
 *  $Author: tanaka $
 *
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "define.h"
#include "solution.h"

solution_t *create_solution(void)
{
  return((solution_t *) calloc(1, sizeof(solution_t)));
}

void free_solution(solution_t *solution)
{
  if(solution != NULL) {
    free(solution->relocation);
    free(solution);
  }
}

void copy_solution(solution_t *dst, solution_t *src)
{
  if(dst != NULL && src != NULL) {
    if(dst->n_block < src->n_relocation) {
      dst->n_block = src->n_block;
      dst->relocation
        = (relocation_t *) realloc((void *) dst->relocation,
                                   (size_t) dst->n_block
                                   *sizeof(relocation_t));
    }
    dst->n_relocation = src->n_relocation;
    memcpy((void *) dst->relocation, (void *) src->relocation,
           (size_t) src->n_relocation*sizeof(relocation_t));
  }
}

void add_relocation(solution_t *solution, int src, int dst, block_t *block)
{
  if(solution != NULL) {
    if(solution->n_block <= solution->n_relocation) {
      solution->n_block += 100;
      solution->relocation
        = (relocation_t *) realloc((void *) solution->relocation,
                                   (size_t) solution->n_block
                                   *sizeof(relocation_t));
    }
    solution->relocation[solution->n_relocation].src = src;
    solution->relocation[solution->n_relocation].dst = dst;
    solution->relocation[solution->n_relocation++].block = *block;
  }
}

state_t *create_state(problem_t *problem)
{
  int i;
  state_t *state = (state_t *) malloc(sizeof(state_t));
  
  state->n_misoverlay = 0;

  state->block
    = (block_t **) malloc((size_t) problem->n_stack*sizeof(block_t *));
  state->block[0]
    = (block_t *) calloc((size_t) problem->n_stack*problem->s_height,
                         sizeof(block_t));
  state->block_state
    = (block_state_t **) malloc((size_t)
                                problem->n_stack*sizeof(block_state_t *));
  state->block_state[0]
    = (block_state_t *) malloc((size_t) problem->n_stack
                               *(problem->s_height + 1)*sizeof(block_state_t));

  for(i = 1; i < problem->n_stack; ++i) {
    state->block[i] = state->block[i - 1] + problem->s_height;
    state->block_state[i]
      = state->block_state[i - 1] + (problem->s_height + 1);
  }

  state->stack = (stack_state_t *) calloc((size_t) problem->n_stack,
                                          sizeof(stack_state_t));
  state->last_relocation
    = (int *) calloc((size_t) problem->n_block, sizeof(int));

  return(state);
}

lb_state_t *create_lb_state(problem_t *problem)
{
  int i;
  lb_state_t *lb_state = (lb_state_t *) calloc(sizeof(lb_state_t), 1);
  
  lb_state->demand = (int *) calloc((size_t) (problem->max_priority + 1)
                                    *(problem->n_stack + 2), sizeof(int));
  lb_state->supply = lb_state->demand + (problem->max_priority + 1);

  lb_state->removal_for_supply
    = (int **) malloc((size_t) problem->n_stack*sizeof(int *));

  lb_state->removal_for_supply[0]
    = lb_state->supply + (problem->max_priority + 1);
  for(i = 1; i < problem->n_stack; ++i) {
    lb_state->removal_for_supply[i]
      = lb_state->removal_for_supply[i - 1] + (problem->max_priority + 1);
  }

  return(lb_state);
}

state_t *initialize_state(problem_t *problem, state_t *state)
{
  int i, j;
  state_t *nstate = (state == NULL)?create_state(problem):state;

  nstate->n_misoverlay = 0;

  memcpy((void *) nstate->block[0], (void *) problem->block[0],
         (size_t) problem->n_stack*problem->s_height*sizeof(block_t));
  memset((void *) nstate->last_relocation, 0, problem->n_block*sizeof(int));

  for(i = 0; i < problem->n_stack; ++i) {
    int priority;
    stack_state_t *stack = &(nstate->stack[i]);
    block_state_t *block_state = nstate->block_state[i];

    stack->n_tier = problem->n_tier[i];
    stack->clean_priority = problem->max_priority;
    stack->n_clean = 0;
    stack->last_change = 0;
    block_state[0].misoverlay_priority = 0;
    block_state[0].upside_down = False;

    for(j = 0; j < stack->n_tier; ++j) {
      priority = nstate->block[i][j].priority;
      if(stack->n_clean == j && stack->clean_priority >= priority) {
        stack->clean_priority = priority;
        ++stack->n_clean;
        block_state[j + 1].misoverlay_priority = 0;
        block_state[j + 1].upside_down = False;
      } else {
        ++nstate->n_misoverlay;
        if(stack->n_clean == j) {
          block_state[j + 1].misoverlay_priority = priority;
          block_state[j + 1].upside_down = True;
        } else if(block_state[j].misoverlay_priority <= priority) {
          block_state[j + 1].misoverlay_priority = priority;
          block_state[j + 1].upside_down = block_state[j].upside_down;
        } else {
          block_state[j + 1].misoverlay_priority
            = block_state[j].misoverlay_priority;
          block_state[j + 1].upside_down = False;
        }
      }
    }

    stack->misoverlay_priority
        = block_state[stack->n_tier].misoverlay_priority;
    stack->upside_down = block_state[stack->n_tier].upside_down;
  }

  return(nstate);
}

lb_state_t *initialize_lb_state(problem_t *problem, state_t *state,
                                lb_state_t *lb_state)
{
  int i, j;
  lb_state_t *nlb_state = (lb_state == NULL)?create_lb_state(problem):lb_state;

  nlb_state->lb = nlb_state->lbBX = nlb_state->lbGX = 0;
  nlb_state->n_dirty_stack = nlb_state->n_full_clean_stack = 0;

  memset((void *) nlb_state->demand, 0,
         (problem->max_priority + 1)*(problem->n_stack + 2)*sizeof(int));

  for(i = 0; i < problem->n_stack; ++i) {
    int priority, count = 0;
    int *removal_for_supply = nlb_state->removal_for_supply[i];
    stack_state_t *stack = &(state->stack[i]);
    block_t *block = state->block[i];

    if(stack->n_clean < stack->n_tier) {
      ++nlb_state->n_dirty_stack;
    } else if(stack->n_tier == problem->s_height) {
      ++nlb_state->n_full_clean_stack;
    }

    for(j = stack->n_clean; j < stack->n_tier; ++j) {
      ++nlb_state->demand[block[j].priority];
    }

    nlb_state->supply[stack->clean_priority]
      += problem->s_height - stack->n_clean;

    priority = 0;
    for(j = stack->n_clean - 1; j >= 0; --j, ++count) {
      for(; priority <= block[j].priority; ++priority) {
        removal_for_supply[priority] = count;
      }
    }
    for(; priority <= problem->max_priority; ++priority) {
      removal_for_supply[priority] = count;
    }
  }

  return(nlb_state);
}

void copy_state(problem_t *problem, state_t *dst, state_t *src)
{
  dst->n_misoverlay = src->n_misoverlay;

  memcpy((void *) dst->block[0], (void *) src->block[0],
         (size_t) problem->n_stack*problem->s_height*sizeof(block_t));
  memcpy((void *) dst->block_state[0], (void *) src->block_state[0],
         (size_t) problem->n_stack*(problem->s_height + 1)
         *sizeof(block_state_t));
  memcpy((void *) dst->stack, (void *) src->stack,
         (size_t) problem->n_stack*sizeof(stack_state_t));

  memcpy((void *) dst->last_relocation, (void *) src->last_relocation,
         (size_t) problem->n_block*sizeof(int));
}

void copy_lb_state(problem_t *problem, lb_state_t *dst, lb_state_t *src)
{
  dst->lb = src->lb;
  dst->lbBX = src->lbBX;
  dst->lbGX = src->lbGX;
  dst->n_dirty_stack = src->n_dirty_stack;
  dst->n_full_clean_stack = src->n_full_clean_stack;

  memcpy((void *) dst->demand, (void *) src->demand,
         (size_t) (problem->max_priority + 1)*(problem->n_stack + 2)
         *sizeof(int));
}

state_t *duplicate_state(problem_t *problem, state_t *state)
{
  state_t *nstate = create_state(problem);
  copy_state(problem, nstate, state);
  return(nstate);
}

lb_state_t *duplicate_lb_state(problem_t *problem, lb_state_t *lb_state)
{
  lb_state_t *nlb_state = create_lb_state(problem);
  copy_lb_state(problem, nlb_state, lb_state);
  return(nlb_state);
}

void free_state(state_t *state)
{
  if(state != NULL) {
    free(state->last_relocation);
    free(state->stack);
    free(state->block_state[0]);
    free(state->block_state);
    free(state->block[0]);
    free(state->block);
    free(state);
  }
}

void free_lb_state(lb_state_t *lb_state)
{
  if(lb_state != NULL) {
    free(lb_state->removal_for_supply);
    free(lb_state->demand);
    free(lb_state);
  }
}

void update_state(problem_t *problem, state_t *state, int src, int dst)
{
  stack_state_t *stack = &(state->stack[src]);
  block_t block = state->block[src][--stack->n_tier];
  block_state_t *block_state = state->block_state[dst];

  if(stack->n_clean > stack->n_tier) {
    /* GX relocation */
    if(stack->n_tier == 0) {
      /* no block is left */
      stack->clean_priority = problem->max_priority;
    } else {      
      stack->clean_priority = state->block[src][stack->n_tier - 1].priority;
    }
    --stack->n_clean;
  } else {
    /* BX relocation */
    --state->n_misoverlay;
    stack->misoverlay_priority
      = state->block_state[src][stack->n_tier].misoverlay_priority;
    stack->upside_down = state->block_state[src][stack->n_tier].upside_down;
  }

  stack = &(state->stack[dst]);
  state->block[dst][stack->n_tier++] = block;

  if(stack->n_clean == stack->n_tier - 1
     && stack->clean_priority >= block.priority) {
    /* XG relocation */
    ++stack->n_clean;
    stack->clean_priority = block.priority;
    block_state[stack->n_tier].misoverlay_priority = 0;
    block_state[stack->n_tier].upside_down = False;
  } else {
    /* XB relocation */
    if(stack->n_clean + 1 == stack->n_tier) {
      block_state[stack->n_tier].misoverlay_priority = block.priority;
      block_state[stack->n_tier].upside_down = True;
    } else if(block_state[stack->n_tier - 1].misoverlay_priority
              <= block.priority) {
      block_state[stack->n_tier].misoverlay_priority = block.priority;
      block_state[stack->n_tier].upside_down
        = block_state[stack->n_tier - 1].upside_down;
    } else {
      block_state[stack->n_tier].misoverlay_priority
        = block_state[stack->n_tier - 1].misoverlay_priority;
      block_state[stack->n_tier].upside_down = False;
    }
    ++state->n_misoverlay;
  }

  stack->misoverlay_priority
    = block_state[stack->n_tier].misoverlay_priority;
  stack->upside_down = block_state[stack->n_tier].upside_down;
}

uchar update_state_src(problem_t *problem, state_t *state,
                       lb_state_t *lb_state, int src, int level)
{
  int i;
  stack_state_t *stack = &(state->stack[src]);
  int priority = state->block[src][--stack->n_tier].priority;
  int *removal_for_supply = lb_state->removal_for_supply[src];

  if(stack->n_clean > stack->n_tier) {
    /* GX relocation */

    if(stack->n_tier == 0) {
      /* no block is left */
      stack->clean_priority = problem->max_priority;
    } else {
      stack->clean_priority = state->block[src][stack->n_tier - 1].priority;
      if(stack->n_tier == problem->s_height - 1) {
        --lb_state->n_full_clean_stack;
      }
    }
    --stack->n_clean;
    stack->last_change = - level;

    /* update suuply */
    lb_state->supply[priority] -= problem->s_height - stack->n_tier - 1;
    lb_state->supply[stack->clean_priority]
      += problem->s_height - stack->n_tier;

    for(i = priority + 1; i <= stack->clean_priority; ++i) {
      --removal_for_supply[i];
    }
    for(; i <= problem->max_priority; ++i) {
      --removal_for_supply[i];
    }

    return(False);
  }

  /* BX relocation */
  --state->n_misoverlay;

  stack->last_change = - level;

  /* demand decreases */
  --lb_state->demand[priority];

  stack->misoverlay_priority
    = state->block_state[src][stack->n_tier].misoverlay_priority;
  stack->upside_down = state->block_state[src][stack->n_tier].upside_down;

  if(stack->n_clean == stack->n_tier) {
    /* the stack becomes clean */
    --lb_state->n_dirty_stack;
  }

  return(True);
}

uchar update_state_dst(problem_t *problem, state_t *state,
                       lb_state_t *lb_state, block_t *block, int dst,
                       int level)
{
  int i;
  stack_state_t *stack = &(state->stack[dst]);
  int *removal_for_supply = lb_state->removal_for_supply[dst];
  block_state_t *block_state = state->block_state[dst];

  state->block[dst][stack->n_tier] = *block;

  if(stack->n_clean == stack->n_tier) {
    /* clean stack */

    if(block->priority <= stack->clean_priority) {
      /* XG relocation */

      if(stack->n_tier == problem->s_height - 1) {
        ++lb_state->n_full_clean_stack;
      }

      /* one block should be retrieved additionally to supply this stack */
      for(i = block->priority + 1; i <= stack->clean_priority; ++i) {
        ++removal_for_supply[i];
      }
      for(; i <= problem->max_priority; ++i) {
        ++removal_for_supply[i];
      }

      /* supply decreases */
      lb_state->supply[stack->clean_priority]
        -= problem->s_height - stack->n_tier;

      ++stack->n_tier;
      ++stack->n_clean;
      stack->clean_priority = block->priority;
      stack->last_change = level;

      block_state[stack->n_tier].misoverlay_priority = 0;
      block_state[stack->n_tier].upside_down = False;

      /* supply increases */
      lb_state->supply[stack->clean_priority]
        += problem->s_height - stack->n_tier;

      return(False);
    }
  }

  /* XB relocation */

  /* demand increases */
  ++lb_state->demand[block->priority];

  if(stack->n_tier == stack->n_clean) {
    /* a clean stack turns dirty */
    ++lb_state->n_dirty_stack;
    block_state[stack->n_tier + 1].misoverlay_priority = block->priority;
    block_state[stack->n_tier + 1].upside_down = True;
  } else if(block_state[stack->n_tier].misoverlay_priority
            <= block->priority) {
    block_state[stack->n_tier + 1].misoverlay_priority = block->priority;
    block_state[stack->n_tier + 1].upside_down
      = block_state[stack->n_tier].upside_down;
  } else {
    block_state[stack->n_tier + 1].misoverlay_priority
      = block_state[stack->n_tier].misoverlay_priority;
    block_state[stack->n_tier + 1].upside_down = False;
  }

  ++stack->n_tier;
  ++state->n_misoverlay;
  stack->misoverlay_priority = block_state[stack->n_tier].misoverlay_priority;
  stack->upside_down = block_state[stack->n_tier].upside_down;
  stack->last_change = level;

  return(True);
}
