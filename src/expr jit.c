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
#include <stdbool.h>

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

struct ej_bytecode {
  uint64_t *ops;
  size_t size;
  size_t capacity;
};

static ej_bytecode *bc_alloc(const size_t cap) {
  ej_bytecode *bc = malloc(sizeof(ej_bytecode));
  bc->ops = malloc(sizeof(uint64_t) * cap);
  bc->size = 0;
  bc->capacity = cap;
  return bc;
}

static void bc_reserve(ej_bytecode *bc, const size_t cap) {
  assert(bc);
  size_t newCap = bc->capacity;
  while (newCap < cap) {
    newCap *= 2;
  }
  if (newCap != bc->capacity) {
    bc->capacity = newCap;
    bc->ops = realloc(bc->ops, newCap);
  }
}

static void bc_push_op(ej_bytecode *bc, const uint64_t op) {
  assert(bc);
  const size_t newSize = bc->size + 1;
  bc_reserve(bc, newSize);
  bc->ops[bc->size] = op;
  bc->size = newSize;
}

static void bc_push_var(ej_bytecode *bc, double *var) {
  bc_push_op(bc, OP_var);
  bc_push_op(bc, *(uint64_t*)(&var));
}

static void bc_push_con(ej_bytecode *bc, double con) {
  bc_push_op(bc, OP_con);
  bc_push_op(bc, *(uint64_t*)(&con));
}

static void bc_push_fun(ej_bytecode *bc, void *addr, size_t arity) {
  assert(arity < 8);
  bc_push_op(bc, OP_fun0 + arity);
  bc_push_op(bc, *(uint64_t*)(&addr));
}

static void bc_push_clo(ej_bytecode *bc, void *addr, size_t arity, void *ctx) {
  assert(arity < 8);
  bc_push_op(bc, OP_fun0 + arity);
  bc_push_op(bc, *(uint64_t*)(&ctx));
  bc_push_op(bc, *(uint64_t*)(&addr));
}

// ( 1/(a+1) + 2/(a+2) + 3/(a+3) )

ej_bytecode *ej_compile(const char *str, ej_variable *vars, size_t len) {
  ej_bytecode *bc = bc_alloc(32);
  
  bc_push_con(bc, 1);
  bc_push_var(bc, (double*)vars[0].addr);
  bc_push_con(bc, 1);
  bc_push_op(bc, OP_add);
  bc_push_op(bc, OP_div);
  
  bc_push_con(bc, 2);
  bc_push_var(bc, (double*)vars[0].addr);
  bc_push_con(bc, 2);
  bc_push_op(bc, OP_add);
  bc_push_op(bc, OP_div);
  
  bc_push_op(bc, OP_add);
  
  bc_push_con(bc, 3);
  bc_push_var(bc, (double*)vars[0].addr);
  bc_push_con(bc, 3);
  bc_push_op(bc, OP_add);
  bc_push_op(bc, OP_div);
  
  bc_push_op(bc, OP_add);
  bc_push_op(bc, OP_ret);
  
  return bc;
}

#define CAST_FUN(...) ((double(*)(__VA_ARGS__))(++op))
#define CAST_CLO(...) ((double(*)(void *, __VA_ARGS__))(++op))
#define CAST_CLO0()   ((double(*)(void *))(++op))
#define CAST_CTX()    (*(void**)(++op))

#define PUSH(VAL)                                                               \
  ++top;                                                                        \
  assert(top < stack + EJ_STACK_SIZE && "Stack overflow");                      \
  *top = VAL
#define POP() *(top--)

double ej_eval(ej_bytecode *bc) {
  assert(bc);
  assert(bc->ops);
  uint64_t *op = bc->ops;
  double stack[EJ_STACK_SIZE];
  double *top = stack;
  
  double x, y;
  void *ctx;
  
  while (1) {
    switch (*op) {
      case OP_neg:
        *top = -(*top);
        break;
      case OP_add:
        y = POP();
        x = *top;
        *top = x + y;
        break;
      case OP_sub:
        y = POP();
        x = *top;
        *top = x - y;
        break;
      case OP_mul:
        y = POP();
        x = *top;
        *top = x * y;
        break;
      case OP_div:
        y = POP();
        x = *top;
        *top = x / y;
        break;
      case OP_mod:
        y = POP();
        x = *top;
        *top = fmod(x, y);
        break;
      case OP_var:
        ++op;
        PUSH(**(double**)op);
        break;
      case OP_con:
        ++op;
        PUSH(*(double*)op);
        break;
      case OP_ret:
        return *top;
      
      case OP_fun0:
        PUSH(CAST_FUN(void)());
        break;
      case OP_fun1:
        PUSH(CAST_FUN(double)(*top));
        --top;
        break;
      case OP_fun2:
        PUSH(CAST_FUN(double, double)(top[-1], top[0]));
        top -= 2;
        break;
      case OP_fun3:
        PUSH(CAST_FUN(double, double, double)(top[-2], top[-1], top[0]));
        top -= 3;
        break;
      case OP_fun4:
        PUSH(CAST_FUN(double, double, double, double)(top[-3], top[-2], top[-1], top[0]));
        top -= 4;
        break;
      case OP_fun5:
        PUSH(CAST_FUN(double, double, double, double, double)(top[-4], top[-3], top[-2], top[-1], top[0]));
        top -= 5;
        break;
      case OP_fun6:
        PUSH(CAST_FUN(double, double, double, double, double, double)(top[-5], top[-4], top[-3], top[-2], top[-1], top[0]));
        top -= 6;
        break;
      case OP_fun7:
        PUSH(CAST_FUN(double, double, double, double, double, double, double)(top[-6], top[-5], top[-4], top[-3], top[-2], top[-1], top[0]));
        top -= 7;
        break;
      
      case OP_clo0:
        ctx = CAST_CTX();
        PUSH(CAST_CLO0()(ctx));
        break;
      case OP_clo1:
        ctx = CAST_CTX();
        PUSH(CAST_CLO(double)(ctx, *top));
        --top;
        break;
      case OP_clo2:
        ctx = CAST_CTX();
        PUSH(CAST_CLO(double, double)(ctx, top[-1], top[0]));
        top -= 2;
        break;
      case OP_clo3:
        ctx = CAST_CTX();
        PUSH(CAST_CLO(double, double, double)(ctx, top[-2], top[-1], top[0]));
        top -= 3;
        break;
      case OP_clo4:
        ctx = CAST_CTX();
        PUSH(CAST_CLO(double, double, double, double)(ctx, top[-3], top[-2], top[-1], top[0]));
        top -= 4;
        break;
      case OP_clo5:
        ctx = CAST_CTX();
        PUSH(CAST_CLO(double, double, double, double, double)(ctx, top[-4], top[-3], top[-2], top[-1], top[0]));
        top -= 5;
        break;
      case OP_clo6:
        ctx = CAST_CTX();
        PUSH(CAST_CLO(double, double, double, double, double, double)(ctx, top[-5], top[-4], top[-3], top[-2], top[-1], top[0]));
        top -= 6;
        break;
      case OP_clo7:
        ctx = CAST_CTX();
        PUSH(CAST_CLO(double, double, double, double, double, double, double)(ctx, top[-6], top[-5], top[-4], top[-3], top[-2], top[-1], top[0]));
        top -= 7;
        break;
        
      default:
        assert(0);
        __builtin_unreachable();
    }
    
    ++op;
  }
}

void ej_free(ej_bytecode *bc) {
  if (bc) {
    free(bc->ops);
    free(bc);
  }
}
