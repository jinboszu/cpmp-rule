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
 *  $Id: solve.c,v 1.39 2017/01/06 10:27:34 tanaka Exp tanaka $
 *  $Revision: 1.39 $
 *  $Date: 2017/01/06 10:27:34 $
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
#include "solve.h"

/* greedy heuristic for easy layouts */
#define HEURISTICS

/* pure branch-and-bound algorithm without outer loop */
#undef PURE_BRANCH_AND_BOUND

/* improved Bortfelt and Forster lower bound */

/* nBX:=(misovalays)+(misoverlay height) if all clean stacks are full */
#define IMPROVED_BF_LOWER_BOUND1

/* nGX:=nGX+1 for several cases */
#define IMPROVED_BF_LOWER_BOUND2

/* nGX:=max(nGX, (nGX for the misoverlaid block with the largest priority)) */
#undef IMPROVED_BF_LOWER_BOUND3

/* improvement of nGX by DP */
#undef IMPROVED_BF_LOWER_BOUND_BY_DP

/* improvement of nGX by checking demand surplus for every priority */
#undef IMPROVED_BF_LOWER_BOUND_BY_ALL

/* second lower bound */
#undef LOWER_BOUND2

/* type of the dominance check for independent relocations */
/* jinbo: use the second type */
// #define TYPE1

/* depth first, ties are broken by best first (smaller lower bound first) */
#define BEST_FIRST

static ulint n_node, count;

typedef struct {
  int index;
  int src;
  int dst;
  int priority;
  stack_state_t dst_stack;
  int n_misoverlay;
#if 1
  int cost;
#endif
} child_node_t;

static child_node_t **child_node;

static state_t *state;
static lb_state_t **lb_state;
static stack_state_t **stack_state;
static solution_t *partial_solution;
static int *lb_work;
static int *last_change_bw, *last_change_empty_bw, *dominance_check;
static int **last_priority_level;
#ifdef TYPE1
static int dominance_table[4][4] =
  { { 0, 1, 0, 0 },
    { 1, 1, 2, 1 },
    { 0, 2, 0, 0 },
    { 0, 1, 0, 0 } };
struct {
  int src;
  int dst;
} preloc[3];
#endif /* TYPE1 */

#ifdef PURE_BRANCH_AND_BOUND
static uchar bb(problem_t *, solution_t *, lb_state_t *, int);
static uchar bb_sub(problem_t *, solution_t *, lb_state_t *, int, int *);
#else /* !PURE_BRANCH_AND_BOUND */
static uchar bb(problem_t *, solution_t *, int *, lb_state_t *, int);
static uchar bb_sub(problem_t *, solution_t *, int *, lb_state_t *, int, int *);
#endif /* !PURE_BRANCH_AND_BOUND */
static int lower_bound(problem_t *, state_t *, lb_state_t *, int, uchar);
#ifdef LOWER_BOUND2
static int lower_bound2(problem_t *, state_t *, lb_state_t *);
#endif /* LOWER_BOUND2 */

uchar solve(problem_t *problem, solution_t *solution)
{
  int i, j, k;
#ifndef PURE_BRANCH_AND_BOUND
  int ub;
#endif /* !PURE_BRANCH_AND_BOUND */
  int n_relocation = MAX_N_RELOCATION + 1;
  int max_n_child = problem->n_stack*(problem->n_stack - 1) + 1;
  int size = (problem->max_priority + 1)*(problem->n_stack + 2);
  uchar ret;
  lb_state_t *clb_state;

  state = initialize_state(problem, NULL);

  if(state->n_misoverlay == 0) {
    fprintf(stderr, "No relocation necessary.\n");
    free_state(state);
    solution->n_relocation = 0;
    return(True);
  }

  /* memory allocation */

  /* working area for LB computation */
#ifdef IMPROVED_BF_LOWER_BOUND_BY_DP
  lb_work
    = (int *) malloc((problem->s_height*problem->n_stack + 1)*sizeof(int));
#else /* !IMPROVED_BF_LOWER_BOUND_BY_DP */
  lb_work = (int *) malloc((problem->s_height + 1)*sizeof(int));
#endif /* !IMPROVED_BF_LOWER_BOUND_BY_DP */

  /* state for LB computation */
  lb_state = (lb_state_t **) malloc(n_relocation*sizeof(lb_state_t *));

  lb_state[0] = (lb_state_t *) malloc(n_relocation*(max_n_child + 1)
                                      *sizeof(lb_state_t));
  lb_state[0][0].demand
    = (int *) calloc((size_t) n_relocation*(max_n_child + 1)*size,
                     sizeof(int));
  lb_state[0][0].removal_for_supply
    = (int **) malloc((size_t) n_relocation*(max_n_child + 1)
                      *problem->n_stack*sizeof(int *));

  for(i = 0; i < n_relocation; ++i) {
    if(i > 0) {
      lb_state[i] = lb_state[i - 1] + (max_n_child + 1);

      lb_state[i][0].demand
        = lb_state[i - 1][0].demand + (max_n_child + 1)*size;
      lb_state[i][0].removal_for_supply
        = lb_state[i - 1][0].removal_for_supply
        + (max_n_child + 1)*problem->n_stack;
    }      

    lb_state[i][0].supply
      = lb_state[i][0].demand + (problem->max_priority + 1);
    lb_state[i][0].removal_for_supply[0]
      = lb_state[i][0].supply + (problem->max_priority + 1);
    for(k = 1; k < problem->n_stack; ++k) {
      lb_state[i][0].removal_for_supply[k]
      = lb_state[i][0].removal_for_supply[k - 1] + (problem->max_priority + 1);
    }
    
    for(j = 1; j < max_n_child + 1; ++j) {
      lb_state[i][j].demand = lb_state[i][j - 1].demand + size;
      lb_state[i][j].supply = lb_state[i][j - 1].supply + size;
      lb_state[i][j].removal_for_supply
        = lb_state[i][j - 1].removal_for_supply + problem->n_stack;
      for(k = 0; k < problem->n_stack; ++k) {
        lb_state[i][j].removal_for_supply[k]
        = lb_state[i][j - 1].removal_for_supply[k] + size;
      }
    }
  }

  child_node
    = (child_node_t **) malloc((size_t) n_relocation*sizeof(child_node_t *));
  child_node[0] = (child_node_t *) malloc((size_t) n_relocation
                                       *max_n_child*sizeof(child_node_t));

  stack_state
    = (stack_state_t **) malloc((size_t) n_relocation*sizeof(stack_state_t *));
  stack_state[0]
    = (stack_state_t *) malloc((size_t) n_relocation*problem->n_stack
                               *sizeof(stack_state_t));

  for(i = 1; i < n_relocation; ++i) {
    child_node[i] = child_node[i - 1] + max_n_child;
    stack_state[i] = stack_state[i - 1] + problem->n_stack;
  }

  clb_state = &(lb_state[0][max_n_child]);

  initialize_lb_state(problem, state, clb_state);

  /* for dominance check */
#ifdef TYPE1
  last_change_bw = (int *) malloc((size_t) 3*problem->n_stack*sizeof(int));
#else /* !TYPE1 */
  last_change_bw
    = (int *) malloc((size_t) (2*problem->n_stack + n_relocation)*sizeof(int));
#endif /* !TYPE1 */
  last_change_empty_bw = last_change_bw + problem->n_stack;
  dominance_check = last_change_bw + 2*problem->n_stack;
  if(problem->duplicate == True) {
    last_priority_level
      = (int **) malloc((size_t) (problem->max_priority + 1)*sizeof(int *));
    last_priority_level[0]
      = (int *) malloc((size_t) (problem->max_priority + 1)*problem->n_stack
                       *sizeof(int));
    for(i = 1; i <= problem->max_priority; ++i) {
      last_priority_level[i] = last_priority_level[i - 1] + problem->n_stack;
    }
  }

  lower_bound(problem, state, clb_state, MAX_N_RELOCATION, False);

#ifdef LOWER_BOUND2
  fprintf(stderr, "initial lb=%d ", clb_state->lb);
  fprintf(stderr, "lb2=%d(%d)\n", lower_bound2(problem, state, clb_state),
          state->n_misoverlay);
#else /* !LOWER_BOUND2 */
  fprintf(stderr, "initial lb=%d\n", clb_state->lb);
#endif /* !LOWER_BOUND2 */
  n_node = 1;

  solution->n_relocation = MAX_N_RELOCATION + 1;

#ifdef HEURISTICS
  if(clb_state->n_dirty_stack + clb_state->n_full_clean_stack
     < problem->n_stack) {
    /* upper bound computation */
    solution->n_relocation = 0;
    if(heuristics(problem, state, solution, MAX_N_RELOCATION + 1)) {
      fprintf(stderr, "initial ub=%d ", solution->n_relocation);
      print_time(problem);
    }
  }
#endif /* HEURISTICS */

  count = 0;
  ret = True;
  partial_solution = create_solution();

#ifdef PURE_BRANCH_AND_BOUND
  ret = bb(problem, solution, clb_state, 1);
#else /* !PURE_BRANCH_AND_BOUND */
  /* main loop */
  for(ub = clb_state->lb; ub < solution->n_relocation; ++ub) {
    fprintf(stderr, "cub=%d ", ub);
    print_time(problem);
    if((ret = bb(problem, solution, &ub, clb_state, 1)) == TimeLimit) {
      break;
    }
  }
#endif /* !PURE_BRANCH_AND_BOUND */

  fprintf(stderr, "nodes=%llu\n", n_node);

  free_solution(partial_solution);
  if(problem->duplicate == True) {
    free(last_priority_level[0]);
    free(last_priority_level);
  }
  free(last_change_bw);
  free(stack_state[0]);
  free(stack_state);
  free(child_node[0]);
  free(child_node);
  free(lb_state[0][0].removal_for_supply);
  free(lb_state[0][0].demand);
  free(lb_state[0]);
  free(lb_state);
  free(lb_work);
  free_state(state);
  heuristics(NULL, NULL, NULL, 0);

  return((ret == TimeLimit)?False:True);
}

#ifdef PURE_BRANCH_AND_BOUND
uchar bb(problem_t *problem, solution_t *solution, lb_state_t *plb_state,
         int level)
#else /* !PURE_BRANCH_AND_BOUND */
uchar bb(problem_t *problem, solution_t *solution, int *ub,
         lb_state_t *plb_state, int level)
#endif /* !PURE_BRANCH_AND_BOUND */
{
  int i, j, k;
  int n_child, last_change;
  int n_misoverlay = state->n_misoverlay;
  uchar ret;
  block_t reloc_block;
  block_state_t block_state_backup;
  block_state_t *block_state;
  stack_state_t *stack = state->stack;
  stack_state_t *stack_backup = stack_state[level];
  stack_state_t src_stack, dst_stack;
  lb_state_t *slb_state = &(lb_state[level][0]);
  child_node_t *cnode = child_node[level];

#ifdef PURE_BRANCH_AND_BOUND
  if(level > MAX_N_RELOCATION) {
    return(False);
  }
#else /* !PURE_BRANCH_AND_BOUND */
  if(level > *ub) {
    return(False);
  }
#endif /* !PURE_BRANCH_AND_BOUND */

  if(tlimit > 0 && ++count == 200000) {
    count = 0;
    if(get_time(problem) >= (double) tlimit) {
      return(TimeLimit);
    }
  }

#if 0
  printf("------\n");
#ifdef PURE_BRANCH_AND_BOUND
  printf("ub=%d, level=%d lb=%d lbGX=%d lbBX=%d\n", solution->n_relocation,
         level, plb_state->lb, plb_state->lbGX, plb_state->lbBX);
#else /* !PURE_BRANCH_AND_BOUND */
  printf("ub=%d, level=%d lb=%d lbGX=%d lbBX=%d\n", *ub, level,
         plb_state->lb, plb_state->lbGX, plb_state->lbBX);
#endif /* !PURE_BRANCH_AND_BOUND */
  print_state(problem, state, stderr);
  print_solution_relocation(problem, partial_solution, stderr);
#endif

  /* generate child nodes */
#ifdef PURE_BRANCH_AND_BOUND
  if((ret = bb_sub(problem, solution, plb_state, level, &n_child))
     != False) {
    return(ret);
  }
#else /* !PURE_BRANCH_AND_BOUND */
  if((ret = bb_sub(problem, solution, ub, plb_state, level, &n_child))
     != False) {
    return(ret);
  }
#endif /* !PURE_BRANCH_AND_BOUND */

  /* branching */
  for(k = 0; k < n_child; ++k) {
    /* bounding (unnecessary) */
#ifdef PURE_BRANCH_AND_BOUND
    if(slb_state[cnode[k].index].lb + level >= solution->n_relocation) {
      continue;
    }
#else /* !PURE_BRANCH_AND_BOUND */
    if(slb_state[cnode[k].index].lb + level > *ub) {
      continue;
    }
#endif /* !PURE_BRANCH_AND_BOUND */

    /* update the information for the child node */
    i = cnode[k].src;
    j = cnode[k].dst;
    src_stack = stack[i];
    dst_stack = stack[j];
    block_state_backup = state->block_state[i][src_stack.n_tier];

    reloc_block = state->block[i][stack[i].n_tier - 1];
    state->block[j][stack[j].n_tier] = reloc_block;

    stack[i] = stack_backup[i];
    stack[j] = cnode[k].dst_stack;
    state->n_misoverlay = cnode[k].n_misoverlay;

    block_state = state->block_state[j];

    if(stack[j].n_clean == stack[j].n_tier) {
      block_state[stack[j].n_tier].misoverlay_priority = 0;
      block_state[stack[j].n_tier].upside_down = False;
    } else if(stack[j].n_clean + 1 == stack[j].n_tier) {
      block_state[stack[j].n_tier].misoverlay_priority = reloc_block.priority;
      block_state[stack[j].n_tier].upside_down = True;
    } else if(reloc_block.priority
              >= block_state[stack[j].n_tier - 1].misoverlay_priority) {
      block_state[stack[j].n_tier].misoverlay_priority = reloc_block.priority;
      block_state[stack[j].n_tier].upside_down
        = block_state[stack[j].n_tier - 1].upside_down;
    } else {
      block_state[stack[j].n_tier].misoverlay_priority
        = block_state[stack[j].n_tier - 1].misoverlay_priority;
      block_state[stack[j].n_tier].upside_down = False;
    }

    /* update the partial solution */
    partial_solution->n_relocation = level - 1;
    add_relocation(partial_solution, i, j, &reloc_block);

    last_change = state->last_relocation[reloc_block.no];
    state->last_relocation[reloc_block.no] = level;

#ifdef PURE_BRANCH_AND_BOUND
    if((ret = bb(problem, solution, &(slb_state[cnode[k].index]),
                 level + 1)) != False) {
      break;
    }
#else /* !PURE_BRANCH_AND_BOUND */
    if((ret = bb(problem, solution, ub, &(slb_state[cnode[k].index]),
                 level + 1)) != False) {
      /* an optimal solution is found, or the time limit is reached */
      break;
    }
#endif /* !PURE_BRANCH_AND_BOUND */

    stack[i] = src_stack;
    stack[j] = dst_stack;
    state->block_state[i][src_stack.n_tier] = block_state_backup;
    
    state->last_relocation[reloc_block.no] = last_change;
    state->n_misoverlay = n_misoverlay;
    state->block[i][stack[i].n_tier - 1] = reloc_block;
  }

  return(ret);
}

#ifdef PURE_BRANCH_AND_BOUND
uchar bb_sub(problem_t *problem, solution_t *solution, lb_state_t *plb_state,
             int level, int *n_child)
#else /* !PURE_BRANCH_AND_BOUND */
uchar bb_sub(problem_t *problem, solution_t *solution, int *ub,
             lb_state_t *plb_state, int level, int *n_child)
#endif /* !PURE_BRANCH_AND_BOUND */
{
#ifdef BEST_FIRST
  int i, j, k;
  int relocation_cost;
#else /* !BEST_FIRST */
  int i, j;
#endif /* !BEST_FIRST */
  int max_n_child = problem->n_stack*(problem->n_stack - 1) + 1;
  int last_change, last_change_fw;
  int prev_priority = -1;
  uchar lb_flag_src, lb_flag_dst;
  block_t reloc_block;
  block_state_t block_state_backup;
#ifdef TYPE1
  int *pdominance_table;
#endif /* !TYPE1 */
  int src_level, dst_level;
  uchar check_flag = (problem->duplicate == True && level >= 2);
  stack_state_t *stack = state->stack;
  /* stack state backup */
  stack_state_t *stack_backup = stack_state[level];
  stack_state_t src_stack, dst_stack;
  /* current state */
  lb_state_t *slb_state = &(lb_state[level][0]);
  /* current */
  lb_state_t *clb_state = slb_state;
  /* backup */
  lb_state_t *blb_state = &(lb_state[level][max_n_child]);
  /* child nodes */
  child_node_t *cnode = child_node[level];

#ifdef TYPE1
  memset((void *) dominance_check, 0, (size_t) problem->n_stack*sizeof(int));
  preloc[0].src = preloc[1].src = preloc[2].src = -1;
#else /* !TYPE1 */
  dominance_check[level - 1] = 0;
#endif /* !TYPE1 */

  if(level >= 2) {
#ifdef TYPE1
    int dst;
    for(i = 0; i < problem->n_stack; ++i) {
      if(stack[i].last_change < 0) {
        last_change = - stack[i].last_change;
        dst = partial_solution->relocation[last_change - 1].dst;
        if(stack[dst].last_change == last_change) {
          if(i > preloc[0].src) {
            preloc[2] = preloc[1];
            preloc[1] = preloc[0];
            preloc[0].src = i;
            preloc[0].dst = dst;
          } else if(i > preloc[1].src) {
            preloc[2] = preloc[1];
            preloc[1].src = i;
            preloc[1].dst = dst;
          } else if(i > preloc[2].src) {
            preloc[2].src = i;
            preloc[2].dst = dst;
          }
        }
      }
    }

    if(preloc[0].src >= 0) {
      dominance_check[preloc[0].src] = dominance_check[preloc[0].dst] = 1;
      if(preloc[1].src >= 0) {
        dominance_check[preloc[1].src] = dominance_check[preloc[1].dst] = 2;
        if(preloc[2].src >= 0) {
          dominance_check[preloc[2].src] = dominance_check[preloc[2].dst] = 3;
        }
      }
    }
#else /* !TYPE1 */
    for(i = level - 2; i >= 0; --i) {
      dominance_check[i]
        = max(dominance_check[i + 1], partial_solution->relocation[i].src);
    }
#endif /* !TYPE1 */
  }

  if(check_flag) {
    int priority;

    memset((void *) last_priority_level[0], 0,
           (size_t) (problem->max_priority + 1)*problem->n_stack*sizeof(int));

    for(i = 0; i < problem->n_stack; ++i) {
      if(stack[i].last_change > 0) {
        last_change = stack[i].last_change;
        priority
          = partial_solution->relocation[last_change - 1].block.priority;
        for(j = 0; j < i; ++j) {
          if(last_priority_level[priority][j] < last_change) {
            last_priority_level[priority][j] = last_change;
          }
        }
      }
    }

    prev_priority = partial_solution->relocation[level - 2].block.priority;
  }

  last_change_bw[problem->n_stack - 1]
    = last_change_empty_bw[problem->n_stack - 1] = level;
  for(i = problem->n_stack - 1; i > 0; --i) {
    last_change = ABS(stack[i].last_change);
    last_change_bw[i - 1] = last_change_bw[i];
    last_change_empty_bw[i - 1] = last_change_empty_bw[i];

    if(stack[i].n_tier < problem->s_height) {
      last_change_bw[i - 1] = min(last_change_bw[i - 1], last_change);
      if(stack[i].n_tier > 0) {
        last_change_empty_bw[i - 1]
          = min(last_change_empty_bw[i - 1], last_change);
      }
    }
  }

  *n_child = 0;
  last_change_fw = level;
  for(i = 0; i < problem->n_stack; ++i) {
    int min_dst_stack = 0;
    uchar empty_stack = False;

    last_change = ABS(stack[i - 1].last_change);
    if(i > 0 && stack[i - 1].n_tier < problem->s_height
       && last_change < last_change_fw) {
      last_change_fw = last_change;
    }

    if(stack[i].n_tier == 0) {
      /* no block */
      continue;
    }

    /* reloc_block: the block to be relocated */
    reloc_block = state->block[i][stack[i].n_tier - 1];

    /* when this block was relocated last */
    last_change = state->last_relocation[reloc_block.no];

    if(last_change > 0) {
      if(stack[partial_solution->relocation[last_change - 1].src]
         .last_change == - last_change) {
        /* x => y, ..., y => * */
        /* x is unchanged during "..." */
        /* ..., x => * strictly dominates x => y, ..., y => * */
        continue;
      }

      /* x => y, ..., y => *, s is changed */
      if(last_change_fw < last_change) {
        /* z is unchanged during "...", and z < y */
        /* x => z, ..., z => * dominates x => y, ..., y => * */
        continue;
      }

      /* jinbo: the case when y is changed is not allowed */
      // if(stack[i].last_change != last_change
      //    && (last_change_empty_bw[i] < last_change
      //        || (stack[i].n_tier > 1 && last_change_bw[i] < last_change))) {
      //   /* y is changed, z is unchanged during "..." */
      //   /* x => z, ..., z => * dominates x => y, ..., y => * */
      //   continue;
      // }
    }

    src_level = ABS(stack[i].last_change);

    if(check_flag) {
      /* same priority */
      /* x => y, ..., z => * */
      /* x, z are unchanged during "...", z < x */
      /* z => y, ..., x => * dominates x => y, ..., z => * */

      /* for speedup */
      if(reloc_block.priority == prev_priority) {
        if(partial_solution->relocation[level - 2].src > i) {
          continue;
        }
        min_dst_stack = partial_solution->relocation[level - 2].dst;
      }

      for(j = i + 1; j < problem->n_stack; ++j) {
        if(stack[j].last_change < 0
           && partial_solution->relocation[- stack[j].last_change - 1]
           .block.priority == reloc_block.priority
           && src_level <= - stack[j].last_change) {
          break;
        }
      }
      if(j < problem->n_stack) {
        continue;
      }
    }

#ifdef TYPE1
    pdominance_table = dominance_table[dominance_check[i]];
#endif /* !TYPE1 */

    /* copy from the parent */
    copy_lb_state(problem, blb_state, plb_state);

    /* backup the state of the source stack */
    src_stack = stack[i];
    block_state_backup = state->block_state[i][src_stack.n_tier];
    /* update the state of the source stack */
    lb_flag_src = update_state_src(problem, state, blb_state, i, level);
    stack_backup[i] = stack[i];

    /* enumerate the candidates for the destination stack */
    for(j = min_dst_stack; j < problem->n_stack; ++j) {
      if(stack[j].n_tier == 0) {
        if(empty_stack == True) {
          /* second empty stack */
          continue;
        }
        empty_stack = True;
      }

      /* destination = source or no space in the destination stack */
      if(j == i || stack[j].n_tier == problem->s_height) {
        /* source and destination stacks are identical, or */
        /* no space is left in the destination stack */
        continue;
      }

      if(stack[j].last_change < last_change
         && - stack[j].last_change < last_change) {
        /* x => y, ..., y => z */
        /* x, y are changed, z is unchanged during "..." */
        /* x => z, ... strictly dominates x => y, ..., y => z */
        continue;
      }

#ifdef TYPE1
      if(i < preloc[pdominance_table[dominance_check[j]]].src) {
        /* x => y, ..., z => u */
        /* x, y are unchanged during "...", z < x */
        /* ..., z =>u, x => y dominates x => y, ..., z => u */
        continue;
      }
#else /* !TYPE1 */
      dst_level = ABS(stack[j].last_change);
      if(i < dominance_check[max(src_level, dst_level)]) {
        /* x => y, ..., z => u */
        /* z, u are unchanged during "...", z < x */
        /* z => u, x => y, ... dominates x => y, ..., z => u */
        continue;
      }
#endif /* !TYPE1 */

      if(check_flag) {
        /* same priority */
        /* x => y, ..., z => u */
        /* y, u are unchanged during "...", y > u */
        /* x => u, ..., z => y dominates x => y, ..., z => u */
#ifdef TYPE1
        dst_level = ABS(stack[j].last_change);
#endif /* !TYPE1 */
        if(dst_level < last_priority_level[reloc_block.priority][j]) {
          continue;
        }

        if(dst_level > 0
           && partial_solution->relocation[dst_level - 1].block.priority
           == reloc_block.priority) {
          if(j == partial_solution->relocation[dst_level - 1].src) {
            /* x => y, ..., z => x */
            /* x, y are unchanged during "..." */
            /* ..., z => y strictly dominates x => y, ..., z => x */
            if(stack[partial_solution->relocation[dst_level - 1].dst]
               .last_change == - dst_level) {
              continue;
            }
            /* x => y, ..., z => x */
            /* x, z are unchanged during "..." */
            /* z => y, ... strictly dominates x => y, ..., z => x */
            if(src_level < dst_level) {
              continue;
            }
          }
        }
      }

      /* backup the state of the destination stack */
      dst_stack = stack[j];
      /* copy from the backup */
      copy_lb_state(problem, clb_state, blb_state);
      /* update the state of the destination stack */
      lb_flag_dst
        = update_state_dst(problem, state, clb_state, &reloc_block, j, level);

      if(state->n_misoverlay == 0) {
        /* solved */

        /* partial solution up to the current node */
        partial_solution->n_relocation = level - 1;
        add_relocation(partial_solution, i, j, &reloc_block);

        copy_solution(solution, partial_solution);
        fprintf(stderr, "ub=%d ", solution->n_relocation);
        print_time(problem);

#ifdef PURE_BRANCH_AND_BOUND
        state->n_misoverlay -= 1 + dst_stack.n_clean - stack[j].n_clean;
        stack[j] = dst_stack;
        continue;
#else /* !PURE_BRANCH_AND_BOUND */
        return(True);
#endif /* !PURE_BRANCH_AND_BOUND */
      }

      /* lower bound */
#ifdef PURE_BRANCH_AND_BOUND
      lower_bound(problem, state, clb_state,
                  solution->n_relocation - level - 1,
                  (lb_flag_src && lb_flag_dst));
#else /* !PURE_BRANCH_AND_BOUND */
      lower_bound(problem, state, clb_state,  *ub - level,
                  (lb_flag_src && lb_flag_dst));
#endif /* !PURE_BRANCH_AND_BOUND */

      ++n_node;

#if 0
      if(n_node > 1U<<30) {
        print_time(problem);
        exit(1);
      }
#endif

      /* bounding */
#ifdef PURE_BRANCH_AND_BOUND
      if(clb_state->lb + level >= solution->n_relocation) {
        /* recover the state */
        state->n_misoverlay -= 1 + dst_stack.n_clean - stack[j].n_clean;
        stack[j] = dst_stack;
        continue;
      }
#else /* !PURE_BRANCH_AND_BOUND */
      if(clb_state->lb + level > *ub) {
        /* recover the state */
        state->n_misoverlay -= 1 + dst_stack.n_clean - stack[j].n_clean;
        stack[j] = dst_stack;
        continue;
      }
#endif /* !PURE_BRANCH_AND_BOUND */

#ifdef LOWER_BOUND2
      lower_bound2(problem, state, clb_state);

      /* bounding */
#ifdef PURE_BRANCH_AND_BOUND
      if(clb_state->lb + level >= solution->n_relocation) {
        /* recover the state */
        state->n_misoverlay -= 1 + dst_stack.n_clean - stack[j].n_clean;
        stack[j] = dst_stack;
        continue;
      }
#else /* !PURE_BRANCH_AND_BOUND */
      if(clb_state->lb + level > *ub) {
        /* recover the state */
        state->n_misoverlay -= 1 + dst_stack.n_clean - stack[j].n_clean;
        stack[j] = dst_stack;
        continue;
      }
#endif /* !PURE_BRANCH_AND_BOUND */

#endif /* LOWER_BOUND2 */

#ifdef HEURISTICS
      if(clb_state->n_dirty_stack + clb_state->n_full_clean_stack
         < problem->n_stack) {
        /* upper bound computation */

        /* partial solution up to the current node */
        partial_solution->n_relocation = level - 1;
        add_relocation(partial_solution, i, j, &reloc_block);

        if(heuristics(problem, state, partial_solution,
                      solution->n_relocation)) {
          /* better upper bound is found */
          copy_solution(solution, partial_solution);
          fprintf(stderr, "ub=%d depth=%d ", solution->n_relocation,
                  level);
          print_time(problem);
#ifndef PURE_BRANCH_AND_BOUND
          if(solution->n_relocation <= *ub) {
            /* When a solution as good as *ub is found, */
            /* the search is terminated */
            return(True);
          }
#endif /* !PURE_BRANCH_AND_BOUND */
        }
      }
#endif /* HEURISTICS */

#if 0
      fprintf(stderr, "lb=%d cub=%d ub=%d\n",
              clb_state->lb + level, *ub, solution->n_relocation);
#endif

      if(dst_stack.n_clean == dst_stack.n_tier) {
        relocation_cost = dst_stack.clean_priority - reloc_block.priority;
      } else {
        relocation_cost = 0;
      }

      cnode[max_n_child - 1].index = *n_child;
      cnode[max_n_child - 1].src = i;
      cnode[max_n_child - 1].dst = j;
      cnode[max_n_child - 1].priority = reloc_block.priority;
      cnode[max_n_child - 1].cost = relocation_cost;
      cnode[max_n_child - 1].n_misoverlay = state->n_misoverlay;
      cnode[max_n_child - 1].dst_stack = stack[j];

      /* insertion sort */
#ifdef BEST_FIRST
      for(k = *n_child - 1; k >= 0
            && (slb_state[cnode[k].index].lb > clb_state->lb
                || (slb_state[cnode[k].index].lb == clb_state->lb
                    && cnode[k].n_misoverlay > state->n_misoverlay)
                || (slb_state[cnode[k].index].lb == clb_state->lb
                    && cnode[k].n_misoverlay == state->n_misoverlay
                    && cnode[k].priority < reloc_block.priority)
                || (slb_state[cnode[k].index].lb == clb_state->lb
                    && cnode[k].n_misoverlay == state->n_misoverlay
                    && cnode[k].priority == reloc_block.priority
                    && cnode[k].cost > relocation_cost));
                --k) {
        cnode[k + 1] = cnode[k];
      }
      cnode[k + 1] = cnode[max_n_child - 1];
#else /* !BEST_FIRST */
      /* no sort */
      cnode[*n_child] = cnode[max_n_child - 1];
#endif /* !BEST_FIRST */

      ++(*n_child);
      ++clb_state;

      /* recover the state of the destination stack */
      state->n_misoverlay -= 1 + dst_stack.n_clean - stack[j].n_clean;
      stack[j] = dst_stack;
    }

    /* recover the state of the source stack */
    state->block[i][stack[i].n_tier] = reloc_block;
    state->n_misoverlay += 1 - src_stack.n_clean + stack[i].n_clean;
    stack[i] = src_stack;
    state->block_state[i][src_stack.n_tier] = block_state_backup;
  }

  return(False);
}

/*
 * Bortfeldt and Forster (2012)
 *
 */
int lower_bound(problem_t *problem, state_t *cstate, lb_state_t *clb_state,
                int upper_bound, uchar flag)
{
  int i;
#ifdef IMPROVED_BF_LOWER_BOUND1
  int min_misoverlay_height = problem->s_height;
#endif /* IMPROVED_BF_LOWER_BOUND1 */
#ifdef IMPROVED_BF_LOWER_BOUND2
  int lb_increment = 0;
#endif /* IMPROVED_BF_LOWER_BOUND2 */

  stack_state_t *stack = cstate->stack;

  clb_state->lbBX = cstate->n_misoverlay;
#if defined(IMPROVED_BF_LOWER_BOUND1) || defined(IMPROVED_BF_LOWER_BOUND2)
  if(clb_state->n_dirty_stack + clb_state->n_full_clean_stack
     == problem->n_stack) {
    /* all the slack stacks are dirty */
    for(i = 0; i < problem->n_stack; ++i) {
      if(min_misoverlay_height > stack[i].n_tier - stack[i].n_clean
         && stack[i].n_clean < stack[i].n_tier) {
        min_misoverlay_height = stack[i].n_tier - stack[i].n_clean;
      }
    }

#ifdef IMPROVED_BF_LOWER_BOUND1
    /* the minimum height of the badly placed blocks is added */
    clb_state->lbBX += min_misoverlay_height;
#endif /* IMPROVED_BF_LOWER_BOUND1 */
  }
#else /* !IMPROVED_BF_LOWER_BOUND1 && !IMPROVED_BF_LOWER_BOUND2 */
  if(clb_state->n_dirty_stack == problem->n_stack) {
    /* all the stacks are dirty */
    int min_misoverlay_height = problem->s_height;

    for(i = 0; i < problem->n_stack; ++i) {
      if(min_misoverlay_height > stack[i].n_tier - stack[i].n_clean) {
        min_misoverlay_height = stack[i].n_tier - stack[i].n_clean;
      }
    }

    /* the minimum height of the badly placed blocks is added */
    clb_state->lbBX += min_misoverlay_height;
  }
#endif /* !IMPROVED_BF_LOWER_BOUND1 && !IMPROVED_BF_LOWER_BOUND2 */

  if(clb_state->lbBX > upper_bound) {
    clb_state->lb = clb_state->lbBX;
    return(clb_state->lb);
  }

  if(flag == False) {
    /* it is necessary to recompute nGX */
    int surplus = 0, max_surplus = 0;
#ifdef IMPROVED_BF_LOWER_BOUND_BY_ALL
    int j;
    int max_lbGX = 0;

    for(i = problem->max_priority; i >= 0; --i) {
      surplus += clb_state->demand[i] - clb_state->supply[i];
      if(surplus > 0 && surplus > max_surplus) {
        /* make space in n stacks */
        int n = (surplus + problem->s_height - 1)/problem->s_height;

        memset((void *) lb_work, 0, (problem->s_height + 1)*sizeof(int));

        for(j = 0; j < problem->n_stack; ++j) {
          if(stack[j].clean_priority < i) {
            /* necessary relocations for accepting demand surplus */
            /* lb_work[i]: the number of stacks such that i blocks */
            /* should be relocated in order to accept demand surplus */
            ++lb_work[clb_state->removal_for_supply[j][i]];
          }
        }

        clb_state->lbGX = 0;
        for(j = 1; n > 0 && j < problem->s_height; n -= lb_work[j], ++j) {
          clb_state->lbGX += j*min(n, lb_work[j]);
        }
        if(max_lbGX < clb_state->lbGX) {
          max_lbGX = clb_state->lbGX;
        }
      }

      if(max_surplus < surplus) {
        max_surplus = surplus;
      }
    }
    clb_state->lbGX = max_lbGX;
#else /* !IMPROVED_BF_LOWER_BOUND_BY_ALL */
#ifdef IMPROVED_BF_LOWER_BOUND3
    int max_misoverlay_priority = -1;
#endif /* IMPROVED_BF_LOWER_BOUND3 */
    int priority = -1;

    clb_state->lbGX = 0;
#ifdef IMPROVED_BF_LOWER_BOUND3
    /* maximum demand surplus */
    for(i = problem->max_priority; clb_state->demand[i] == 0; --i) {
      surplus -= clb_state->supply[i];
    }
    max_misoverlay_priority = i;

    for(; i >= 0; --i) {
      surplus += clb_state->demand[i] - clb_state->supply[i];
      if(max_surplus < surplus) {
        priority = i;
        max_surplus = surplus;
      }
    }
#else /* !IMPROVED_BF_LOWER_BOUND3 */
    for(i = problem->max_priority; i >= 0; --i) {
      surplus += clb_state->demand[i] - clb_state->supply[i];
      if(max_surplus < surplus) {
        priority = i;
        max_surplus = surplus;
      }
    }
#endif /* !IMPROVED_BF_LOWER_BOUND3 */

    if(max_surplus > 0) {
      /* make space in n stacks */
#ifdef IMPROVED_BF_LOWER_BOUND_BY_DP
      memset((void *) lb_work, 0,
             (problem->s_height*problem->n_stack + 1)*sizeof(int));

      lb_work[0] = 1;
      for(i = 0; i < problem->n_stack; ++i) {
        if(stack[i].clean_priority < priority) {
          int j, k;
          int n_removal = clb_state->removal_for_supply[i][priority];
          int n_slot = problem->s_height - stack[i].n_clean + n_removal;

          for(j = max_surplus - 1; j >= 0; --j) {
            if(lb_work[j] > 0) {
              for(k = 0; k <= problem->s_height - n_slot; ++k) {
                if(lb_work[j + n_slot + k] == 0) {
                  lb_work[j + n_slot + k] = lb_work[j] + n_removal + k;
                } else {
                  lb_work[j + n_slot + k]
                    = min(lb_work[j + n_slot + k], lb_work[j] + n_removal + k);
                }
              }
            }
          }
        }
      }
      
      clb_state->lbGX = problem->n_block;
      for(i = max_surplus; i < max_surplus + problem->s_height; ++i) {
        if(lb_work[i] > 0 && clb_state->lbGX > lb_work[i]) {
          clb_state->lbGX = lb_work[i];
        }
      }
      --clb_state->lbGX;
#else /* !IMPROVED_BF_LOWER_BOUND_BY_DP */
      int n = (max_surplus + problem->s_height - 1)/problem->s_height;
#ifdef IMPROVED_BF_LOWER_BOUND3
      int lbGX = problem->s_height;
#endif /* IMPROVED_BF_LOWER_BOUND3 */

      memset((void *) lb_work, 0, (problem->s_height + 1)*sizeof(int));

      for(i = 0; i < problem->n_stack; ++i) {
#ifdef IMPROVED_BF_LOWER_BOUND3
        if(lbGX > clb_state->removal_for_supply[i][max_misoverlay_priority]) {
          lbGX = clb_state->removal_for_supply[i][max_misoverlay_priority];
        }
#endif /* IMPROVED_BF_LOWER_BOUND3 */
        if(stack[i].clean_priority < priority) {
          /* necessary relocations for accepting demand surplus */
          /* lb_work[i]: the number of stacks such that i blocks */
          /* should be relocated in order to accept demand surplus */
          ++lb_work[clb_state->removal_for_supply[i][priority]];
        }
      }

      for(i = 1; n > 0 && i < problem->s_height; n -= lb_work[i], ++i) {
        clb_state->lbGX += i*min(n, lb_work[i]);
      }

#ifdef IMPROVED_BF_LOWER_BOUND3
      if(lbGX > clb_state->lbGX) {
        clb_state->lbGX = lbGX;
      }
#endif /* IMPROVED_BF_LOWER_BOUND3 */
#endif /* !IMPROVED_BF_LOWER_BOUND_BY_DP */
    }
#endif /* !IMPROVED_BF_LOWER_BOUND_BY_ALL */
  }

  clb_state->lb = clb_state->lbBX + clb_state->lbGX;

#ifdef IMPROVED_BF_LOWER_BOUND2
  if(clb_state->lb > upper_bound) {
    return(clb_state->lb);
  }

  if(clb_state->n_dirty_stack == problem->n_stack) {
    /* no slack clean stack */
    int max_clean_priority = -1;
    /* maximum priority of clean stacks having the minimum misoverlay height */
    for(i = 0; i < problem->n_stack; ++i) {
      if(stack[i].n_tier - stack[i].n_clean == min_misoverlay_height
         && max_clean_priority < stack[i].clean_priority) {
        max_clean_priority = stack[i].clean_priority;
      }
    }
    lb_increment = 1;
    for(i = 0; i < problem->n_stack; ++i) {
      if(stack[i].upside_down == True
         && stack[i].misoverlay_priority <= max_clean_priority) {
        lb_increment = 0;
        break;
      }
    }
  } else if(clb_state->n_dirty_stack == problem->n_stack - 1) {
    /* only one clean stack */
    int s;

    for(s = 0; s < problem->n_stack && stack[s].n_clean < stack[s].n_tier;
        ++s);

    if(clb_state->n_full_clean_stack > 0) {
      /* clean stack is full */
      int max_clean_priority = -1;

      /* the maximum clean priority of stacks having the minimum misoverlay */
      /* height. these stacks are candidates for next cleaning */
      for(i = 0; i < problem->n_stack; ++i) {
        if(min_misoverlay_height == stack[i].n_tier - stack[i].n_clean
           && max_clean_priority < stack[i].clean_priority) {
          max_clean_priority = stack[i].clean_priority;
        }
      }

      if(max_clean_priority < stack[s].clean_priority) {
        /* unable to make space in the clean stack */
        lb_increment = 1;
        for(i = 0; i < problem->n_stack; ++i) {
          if(stack[i].upside_down == True
             && stack[i].misoverlay_priority <= max_clean_priority) {
            lb_increment = 0;
            break;
          }
        }
      } else {
        int priority = cstate->block[s][stack[s].n_tier - 1].priority;
        /* relocate some blocks from the clean stack */
        /* to the stack cleaned next*/
        for(i = stack[s].n_tier - 2;
            i >= 0 && cstate->block[s][i].priority == priority; --i);
        if(i >= 0) {
          /* the clean stack is not empty */
          if(max_clean_priority < cstate->block[s][i].priority) {
            max_clean_priority = cstate->block[s][i].priority;
          }
          if(max_clean_priority < problem->max_priority) {
            lb_increment = 1;
            for(i = 0; i < problem->n_stack; ++i) {
              if(stack[i].n_clean < stack[i].n_tier
                 && stack[i].misoverlay_priority <= max_clean_priority) {
              lb_increment = 0;
              break;
              }
            }
          }
        }
      }
    } else {
      /* the clean stack is not full */
      lb_increment = 1;
      for(i = 0; i < problem->n_stack; ++i) {
        if(stack[i].upside_down == True
           && stack[i].misoverlay_priority <= stack[s].clean_priority) {
          lb_increment = 0;
          break;
        }
      }
    }
  } else if(clb_state->lbGX == 0
            && clb_state->n_dirty_stack + clb_state->n_full_clean_stack
            == problem->n_stack - 1) {
    /* only one clean stack is slack */
    int max_clean_priority = problem->max_priority;
    for(i = 0; i < problem->n_stack; ++i) {
      if(stack[i].n_tier == stack[i].n_clean
         && stack[i].n_tier < problem->s_height) {
        max_clean_priority = stack[i].clean_priority;
        break;
      }
    }

    lb_increment = 1;
    for(i = 0; i < problem->n_stack; ++i) {
      if(stack[i].upside_down == True
         && stack[i].misoverlay_priority <= max_clean_priority) {
        lb_increment = 0;
          break;
      }
    }
  } else if(clb_state->n_dirty_stack == problem->n_stack - 2
            && clb_state->n_full_clean_stack < 2) {
    /* two clean stacks */
    int s1, s2;
    int max_clean_priority = problem->max_priority;
    uchar lb_flag = False;

    for(s1 = 0; stack[s1].n_tier != stack[s1].n_clean; ++s1);
    for(s2 = s1 + 1; stack[s2].n_tier != stack[s2].n_clean; ++s2);

    if(stack[s1].clean_priority < stack[s2].clean_priority) {
      /* topmost block of stack s1 can be relocated to stack j */
      if(stack[s2].n_tier == problem->s_height) {
        /* stack s2 is full,  only stack s1 is available */
        max_clean_priority = stack[s1].clean_priority;
        lb_flag = True;
      } else if(stack[s1].n_tier > 1) {
        int j
          = max(0, stack[s1].n_tier + stack[s2].n_tier - problem->s_height);

        /* relocate blocks from stack s1 to stack s2 */
        for(i = stack[s1].n_tier - 2;
            i >= j
              && stack[s1].clean_priority == cstate->block[s1][i].priority;
            --i);
        if(i >= 0) {
          /* stack s1 is not empty */
          max_clean_priority = max(cstate->block[s1][i].priority,
                                   stack[s2].clean_priority);
        }
      }
    } else if(stack[s1].clean_priority > stack[s2].clean_priority) {
      if(stack[s1].n_tier == problem->s_height) {
        max_clean_priority = stack[s2].clean_priority;
        lb_flag = True;
      } else if(stack[s2].n_tier > 1) {
        int j
          = max(0, stack[s1].n_tier + stack[s2].n_tier - problem->s_height);
        for(i = stack[s2].n_tier - 2;
            i >= j && stack[s2].clean_priority == cstate->block[s2][i].priority;
            --i);
        if(i >= 0) {
          max_clean_priority = max(cstate->block[s2][i].priority,
                                   stack[s1].clean_priority);
        }
      }
    } else if(stack[s1].n_tier > 1 && stack[s2].n_tier > 1) {
      /* stacks s1 and s2 have the same priority */
      /* relocate blocks from stack i to stack j */
      int j = max(0, stack[s1].n_tier + stack[s2].n_tier - problem->s_height);

      /* relocate blocks from stack s1 to stack s2 */
      for(i = stack[s1].n_tier - 2;
          i >= j && stack[s1].clean_priority == cstate->block[s1][i].priority;
          --i);
      if(i >= 0) {
        max_clean_priority = cstate->block[s1][i].priority;
        /* relocate blocks from stack s2 to stack s1 */
        for(i = stack[s2].n_tier - 2;
            i >= j && stack[s2].clean_priority == cstate->block[s2][i].priority;
            --i);
        if(i < 0) {
          max_clean_priority = problem->max_priority;
        } else if(max_clean_priority < cstate->block[s2][i].priority) {
          max_clean_priority = cstate->block[s2][i].priority;
        }
      }
    }

    if(lb_flag == True) {
      /* only one stack */
      lb_increment = 1;
      for(i = 0; i < problem->n_stack; ++i) {
        if(stack[i].upside_down == True
           && stack[i].misoverlay_priority <= max_clean_priority) {
          lb_increment = 0;
          break;
        }
      }
    } else if(max_clean_priority < problem->max_priority) {
      lb_increment = 1;
      for(i = 0; i < problem->n_stack; ++i) {
        if(stack[i].n_clean < stack[i].n_tier
           && stack[i].misoverlay_priority <= max_clean_priority) {
          lb_increment = 0;
          break;
        }
      }
    }
  }

  clb_state->lb += lb_increment;
#endif /* IMPROVED_BF_LOWER_BOUND2 */

  return(clb_state->lb);
}

#ifdef LOWER_BOUND2
int lower_bound2(problem_t *problem, state_t *cstate, lb_state_t *clb_state)
{
  int i, j, k, l;
  int min_relocation = problem->s_height;
  int max_clean_priority = -1;
  int misoverlay_priority;
  stack_state_t *stack = cstate->stack;

  for(i = 0; i < problem->n_stack; ++i) {
    if(max_clean_priority < stack[i].clean_priority) {
      max_clean_priority = stack[i].clean_priority;
    }
  }

  if(clb_state->n_dirty_stack == problem->n_stack) {
    /* all the stacks are dirty */
    for(i = 0; i < problem->n_stack; ++i) {
      misoverlay_priority = stack[i].misoverlay_priority;

      for(j = 0; j < problem->n_stack; ++j) {
        k = clb_state->removal_for_supply[j][misoverlay_priority];
        l = 0;
        if(stack[j].n_clean > k
           && cstate->block[j][stack[j].n_clean - k - 1].priority
           > max_clean_priority) {
          l = 1;
        }
          
        if(i == j) {
          if(stack[j].n_tier - stack[j].n_clean
             > clb_state->lbBX - cstate->n_misoverlay) {
            ++l;
          }
        } else if(stack[j].misoverlay_priority > max_clean_priority) {
          l = 1;
        }

        if(min_relocation > k + l) {
          min_relocation = k + l;
          if(clb_state->lbBX + min_relocation < clb_state->lb) {
            i = problem->n_stack - 1;
            break;
          }
        }
      }
    }

    min_relocation += clb_state->lbBX - cstate->n_misoverlay;
    clb_state->lb = max(clb_state->lb, cstate->n_misoverlay + min_relocation);
  } else {
    for(i = 0; i < problem->n_stack; ++i) {
      if(stack[i].n_tier == 0) {
        min_relocation = 0;
        break;
      }

      misoverlay_priority = stack[i].misoverlay_priority;
      if(misoverlay_priority > 0) {
        for(j = 0; j < problem->n_stack; ++j) {
          k = clb_state->removal_for_supply[j][misoverlay_priority];
          if(i != j && stack[j].n_clean < stack[j].n_tier
             && stack[i].misoverlay_priority > max_clean_priority) {
            ++k;
          } else if(stack[j].n_clean > k
                    && cstate->block[j][stack[j].n_clean - k - 1].priority
                    > max_clean_priority) {
            ++k;
          }

          if(i == j) {
            ++k;
          }

          if(min_relocation > k) {
            min_relocation = k;
            if(cstate->n_misoverlay + min_relocation < clb_state->lb) {
              i = problem->n_stack - 1;
              break;
            }
          }
        }
      }
    }

    clb_state->lb = max(clb_state->lb, cstate->n_misoverlay + min_relocation);
  }

  return(cstate->n_misoverlay + min_relocation);
}
#endif /* LOWER_BOUND2 */
