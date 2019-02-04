//
//  expr jit.c
//  Expr JIT
//
//  Created by Indi Kernick on 5/2/19.
//  Copyright © 2019 Indi Kernick. All rights reserved.
//

#include "expr jit.h"

#include <math.h>
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>

enum {
  OP_neg = 0,
  OP_add,
  OP_sub,
  OP_mul,
  OP_div,
  OP_mod,
  OP_var,
  OP_con,
  OP_ret,
  
  OP_fun0,
  OP_fun1,
  OP_fun2,
  OP_fun3,
  OP_fun4,
  OP_fun5,
  OP_fun6,
  OP_fun7,
  
  OP_clo0,
  OP_clo1,
  OP_clo2,
  OP_clo3,
  OP_clo4,
  OP_clo5,
  OP_clo6,
  OP_clo7
};

struct ByteCode {
  uint64_t *ops;
};

ByteCode *ej_compile(const char *str, Variable *vars, size_t len) {
  assert(str);
  return NULL;
}

typedef struct Stack {
  double *base;
  double *top;
  double *roof;
} Stack;

static Stack makeStack(const size_t cap) {
  double *base = malloc(cap);
  Stack stack = {base, base, base + (cap - 1)};
  return stack;
}

static void freeStack(Stack *stack) {
  assert(stack);
  free(stack->base);
}

static void push(Stack *stack, const double val) {
  assert(stack);
  if (__builtin_expect(stack->top == stack->roof, 0)) {
    const ptrdiff_t capacity = stack->roof - stack->base + 1;
    const ptrdiff_t newCap = capacity * 2;
    stack->base = realloc(stack->base, newCap);
    stack->top = stack->base + capacity;
    stack->roof = stack->base + newCap;
  }
  *stack->top = val;
  ++stack->top;
}

static double pop(Stack *stack) {
  assert(stack);
  const double top = *stack->top;
  --stack->top;
  return top;
}

#define CAST_FUN(...) ((double(*)(__VA_ARGS__))op)
#define CAST_CLO(...) ((double(*)(void *, __VA_ARGS__))op)
#define CAST_CLO0()   ((double(*)(void *))op)

double ej_eval(ByteCode *bc) {
  assert(bc);
  assert(bc->ops);
  uint64_t *op = bc->ops;
  Stack stack = makeStack(32);
  
  double x, y;
  void *ctx;
  
  while (1) {
    switch (*op) {
      case OP_neg:
        *stack.top = -(*stack.top);
        break;
      case OP_add:
        y = *stack.top;
        x = pop(&stack);
        *stack.top = x + y;
        break;
      case OP_sub:
        y = *stack.top;
        x = pop(&stack);
        *stack.top = x - y;
        break;
      case OP_mul:
        y = *stack.top;
        x = pop(&stack);
        *stack.top = x * y;
        break;
      case OP_div:
        y = *stack.top;
        x = pop(&stack);
        *stack.top = x / y;
        break;
      case OP_mod:
        y = *stack.top;
        x = pop(&stack);
        *stack.top = fmod(x, y);
        break;
      case OP_var:
        ++op;
        push(&stack, **(double**)op);
        break;
      case OP_con:
        ++op;
        push(&stack, *(double*)op);
        break;
      case OP_ret:
        x = pop(&stack);
        freeStack(&stack);
        return x;
      
      case OP_fun0:
        ++op;
        push(&stack, CAST_FUN(void)());
        break;
      case OP_fun1:
        ++op;
        push(&stack, CAST_FUN(double)(*stack.top));
        --stack.top;
        break;
      case OP_fun2:
        ++op;
        push(&stack, CAST_FUN(double, double)(stack.top[-1], stack.top[0]));
        stack.top -= 2;
        break;
      
      case OP_clo0:
        ++op;
        ctx = *(void**)op;
        ++op;
        push(&stack, CAST_CLO0()(ctx));
        break;
      case OP_clo1:
        ++op;
        ctx = *(void**)op;
        ++op;
        push(&stack, CAST_CLO(double)(ctx, *stack.top));
        --stack.top;
        break;
      case OP_clo2:
        ++op;
        ctx = *(void**)op;
        ++op;
        push(&stack, CAST_CLO(double, double)(ctx, stack.top[-1], stack.top[0]));
        stack.top -= 2;
        break;
        
      default:
        assert(0);
        __builtin_unreachable();
    }
    
    ++op;
  }
}

void ej_free(ByteCode *bc) {
  if (bc) {
    free(bc->ops);
    free(bc);
  }
}
