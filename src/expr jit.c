//
//  expr jit.c
//  Expr JIT
//
//  Created by Indi Kernick on 5/2/19.
//  Copyright © 2019 Indi Kernick. All rights reserved.
//

#include "expr jit.h"

#include <math.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>

enum {
  OP_neg = 0,
  OP_add,
  OP_sub,
  OP_mul,
  OP_div,
  OP_var,
  OP_con,
  OP_ret,
  
  OP_fun0 = 8,
  OP_fun1,
  OP_fun2,
  OP_fun3,
  OP_fun4,
  OP_fun5,
  OP_fun6,
  OP_fun7,
  
  OP_clo0 = 16,
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

static void bc_push_op(ej_bytecode *bc, const uint64_t op) {
  assert(bc);
  const size_t size = bc->size;
  const size_t capacity = bc->capacity;
  if (size == capacity) {
    const size_t newCapacity = capacity * 2;
    bc->capacity = newCapacity;
    bc->ops = realloc(bc->ops, newCapacity);
  }
  const size_t newSize = size + 1;
  bc->ops[size] = op;
  bc->size = newSize;
}

static void bc_push_var(ej_bytecode *bc, const double *var) {
  bc_push_op(bc, OP_var);
  bc_push_op(bc, *(uint64_t*)(&var));
}

static void bc_push_con(ej_bytecode *bc, double con) {
  bc_push_op(bc, OP_con);
  bc_push_op(bc, *(uint64_t*)(&con));
}

static void bc_push_fun(ej_bytecode *bc, const void *addr, size_t arity) {
  assert(arity < 8);
  bc_push_op(bc, OP_fun0 + arity);
  bc_push_op(bc, *(uint64_t*)(&addr));
}

static void bc_push_clo(ej_bytecode *bc, const void *addr, size_t arity, void *ctx) {
  assert(arity < 8);
  bc_push_op(bc, OP_clo0 + arity);
  bc_push_op(bc, *(uint64_t*)(&ctx));
  bc_push_op(bc, *(uint64_t*)(&addr));
}

enum {
  OPER_inflix,
  OPER_prefix,
  OPER_paren
};

enum {
  ASSOC_left,
  ASSOC_right
};

typedef struct oper {
  const char *name;
  int prec;
  int assoc;
  int type;
  const void *addr;
  void *ctx;
} oper;


static oper prec_table[] = {
  {"-", 4, ASSOC_right, OPER_prefix},
  {"^", 3, ASSOC_right, OPER_inflix},
  {"%", 2, ASSOC_left, OPER_inflix},
  {"/", 2, ASSOC_left, OPER_inflix},
  {"*", 2, ASSOC_left, OPER_inflix},
  {"-", 1, ASSOC_left, OPER_inflix},
  {"+", 1, ASSOC_left, OPER_inflix},
};

static const double constant_e = 2.71828182845904523536;
static const double constant_pi = 3.14159265358979323846;

static ej_variable builtins[] = {
  {"abs", fabs, EJ_FUN1, NULL},
  {"sqrt", sqrt, EJ_FUN1, NULL},
  {"e", &constant_e, EJ_VAR, NULL},
  {"pi", &constant_pi, EJ_VAR, NULL}
};

typedef struct oper_stack {
  oper *data;
  size_t size;
  size_t capacity;
} oper_stack;

static oper_stack os_alloc(const size_t cap) {
  oper_stack stack = {malloc(cap * sizeof(oper)), 0, cap};
  return stack;
}

static void os_free(oper_stack *stack) {
  assert(stack);
  free(stack->data);
}

static oper *os_top(oper_stack *stack) {
  assert(stack);
  assert(stack->size);
  return stack->data + (stack->size - 1);
}

static void os_pop(oper_stack *stack) {
  assert(stack);
  assert(stack->size);
  stack->size--;
}

static void os_push(oper_stack *stack, oper op) {
  assert(stack);
  const size_t size = stack->size;
  const size_t capacity = stack->capacity;
  if (size == capacity) {
    const size_t newCapacity = capacity * 2;
    stack->capacity = newCapacity;
    stack->data = realloc(stack->data, newCapacity);
  }
  const size_t newSize = size + 1;
  stack->data[size] = op;
  stack->size = newSize;
}

static ej_variable *findVar(ej_variable *vars, size_t len, const char *ident, size_t identSize) {
  for (; len != 0; --len, ++vars) {
    if (strncmp(vars->name, ident, identSize) == 0) {
      return vars;
    }
  }
  return NULL;
}

static ej_variable *findBuiltin(const char *ident, size_t identSize) {
  return findVar(builtins, sizeof(builtins) / sizeof(builtins[0]), ident, identSize);
}

static oper *findOper(const char op, int type) {
  oper *row = prec_table;
  size_t size = sizeof(prec_table) / sizeof(prec_table[0]);
  for (; size != 0; --size, ++row) {
    if (row->name[0] == op && row->name[1] == 0 && row->type == type) {
      return row;
    }
  }
  return NULL;
}

static bool funOrClo(const int type) {
  return (type & EJ_FUN) == EJ_FUN || (type & EJ_CLO) == EJ_CLO;
}

static bool shouldPop(oper *top, oper *other) {
  if (top->type == OPER_paren) return false;
  if ((top->type & EJ_FUN) == EJ_FUN) return true;
  if ((top->type & EJ_CLO) == EJ_CLO) return true;
  if (top->prec > other->prec) return true;
  if (top->prec == other->prec && top->assoc == ASSOC_left) return true;
  return false;
}

#define ARITY(TYPE) ((TYPE) & 7)

static void pushToOutput(ej_bytecode *bc, oper *op) {
  if (op->type == OPER_inflix) {
    if (op->name[0] == '+') {
      bc_push_op(bc, OP_add);
    } else if (op->name[0] == '-') {
      bc_push_op(bc, OP_sub);
    } else if (op->name[0] == '*') {
      bc_push_op(bc, OP_mul);
    } else if (op->name[0] == '/') {
      bc_push_op(bc, OP_div);
    } else if (op->name[0] == '%') {
      bc_push_fun(bc, fmod, 2);
    } else if (op->name[0] == '^') {
      bc_push_fun(bc, pow, 2);
    } else {
      assert(false);
    }
  } else if (op->type == OPER_prefix) {
    if (op->name[0] == '-') {
      bc_push_op(bc, OP_neg);
    } else {
      assert(false);
    }
  } else if ((op->type & EJ_FUN) == EJ_FUN) {
    bc_push_fun(bc, op->addr, ARITY(op->type));
  } else if ((op->type & EJ_CLO) == EJ_CLO) {
    bc_push_clo(bc, op->addr, ARITY(op->type), op->ctx);
  } else {
    assert(false);
  }
}

static void pushOutputUntilParen(ej_bytecode *bc, oper_stack *stack) {
  assert(stack->size && "Unmatching parentheses");
  oper *top = os_top(stack);
  while (top->type != OPER_paren) {
    pushToOutput(bc, top);
    os_pop(stack);
    assert(stack->size && "Unmatching parentheses");
    top = os_top(stack);
  }
}

static void pushToOper(ej_bytecode *bc, oper_stack *stack, oper *op) {
  if (stack->size) {
    oper *top = os_top(stack);
    while (shouldPop(top, op)) {
      pushToOutput(bc, top);
      os_pop(stack);
      top = os_top(stack);
    }
  }
  os_push(stack, *op);
}

ej_bytecode *ej_compile(const char *str, ej_variable *vars, size_t len) {
  assert(str);
  assert(vars ? len != 0 : len == 0);
  
  ej_bytecode *bc = bc_alloc(64);
  oper_stack stack = os_alloc(16);
  bool prefixContext = true;
  
  while (*str) {
    while (isspace(*str)) ++str;
    
    if (isalpha(*str)) {
      const char *begin = str++;
      while (isalnum(*str)) ++str;
      ej_variable *var = findVar(vars, len, begin, str - begin);
      if (!var) {
        var = findBuiltin(begin, str - begin);
      }
      assert(var && "Failed to lookup identifier");
      while (isspace(*str)) ++str;
      if (*str == '(') {
        assert(funOrClo(var->type) && "Calling a variable");
        oper op;
        op.name = var->name;
        op.prec = 0;
        op.assoc = 0;
        op.type = var->type;
        op.addr = var->addr;
        op.ctx = var->ctx;
        os_push(&stack, op);
      } else {
        assert(var->type == EJ_VAR && "Taking the value of a function");
        bc_push_var(bc, var->addr);
        prefixContext = false;
      }
      continue;
    }
    
    if (*str == '(') {
      ++str;
      oper op;
      op.name = "(";
      op.prec = 0;
      op.assoc = 0;
      op.type = OPER_paren;
      os_push(&stack, op);
      prefixContext = true;
      continue;
    }
    
    if (*str == ',') {
      ++str;
      pushOutputUntilParen(bc, &stack);
      prefixContext = true;
      continue;
    }
    
    if (*str == ')') {
      ++str;
      pushOutputUntilParen(bc, &stack);
      os_pop(&stack);
      prefixContext = false;
      continue;
    }
    
    if (prefixContext) {
      oper *op = findOper(*str, OPER_prefix);
      if (op) {
        str += strlen(op->name);
        pushToOper(bc, &stack, op);
        prefixContext = true;
        continue;
      }
    }
    
    oper *op = findOper(*str, OPER_inflix);
    if (op) {
      str += strlen(op->name);
      pushToOper(bc, &stack, op);
      prefixContext = false;
      continue;
    }
    
    char *end;
    double number = strtod(str, &end);
    if (end != str) {
      str = end;
      bc_push_con(bc, number);
      prefixContext = false;
      continue;
    }
    
    assert(false);
  }
  
  while (stack.size) {
    oper *top = os_top(&stack);
    assert(top->type != OPER_paren && "Unmatching parentheses");
    pushToOutput(bc, top);
    os_pop(&stack);
  }
  
  bc_push_op(bc, OP_ret);
  os_free(&stack);
  return bc;
}

#define CAST_FUN(...) (*(double(**)(__VA_ARGS__))(++op))
#define CAST_CLO(...) (*(double(**)(void *, __VA_ARGS__))(++op))
#define CAST_CLO0()   (*(double(**)(void *))(++op))
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
        x = CAST_FUN(double)(*top);
        --top;
        PUSH(x);
        break;
      case OP_fun2:
        x = CAST_FUN(double, double)(top[-1], top[0]);
        top -= 2;
        PUSH(x);
        break;
      case OP_fun3:
        x = CAST_FUN(double, double, double)(top[-2], top[-1], top[0]);
        top -= 3;
        PUSH(x);
        break;
      case OP_fun4:
        x = CAST_FUN(double, double, double, double)(top[-3], top[-2], top[-1], top[0]);
        top -= 4;
        PUSH(x);
        break;
      case OP_fun5:
        x = CAST_FUN(double, double, double, double, double)(top[-4], top[-3], top[-2], top[-1], top[0]);
        top -= 5;
        PUSH(x);
        break;
      case OP_fun6:
        x = CAST_FUN(double, double, double, double, double, double)(top[-5], top[-4], top[-3], top[-2], top[-1], top[0]);
        top -= 6;
        PUSH(x);
        break;
      case OP_fun7:
        x = CAST_FUN(double, double, double, double, double, double, double)(top[-6], top[-5], top[-4], top[-3], top[-2], top[-1], top[0]);
        top -= 7;
        PUSH(x);
        break;
      
      case OP_clo0:
        ctx = CAST_CTX();
        PUSH(CAST_CLO0()(ctx));
        break;
      case OP_clo1:
        ctx = CAST_CTX();
        x = CAST_CLO(double)(ctx, *top);
        --top;
        PUSH(x);
        break;
      case OP_clo2:
        ctx = CAST_CTX();
        x = CAST_CLO(double, double)(ctx, top[-1], top[0]);
        top -= 2;
        PUSH(x);
        break;
      case OP_clo3:
        ctx = CAST_CTX();
        x = CAST_CLO(double, double, double)(ctx, top[-2], top[-1], top[0]);
        top -= 3;
        PUSH(x);
        break;
      case OP_clo4:
        ctx = CAST_CTX();
        x = CAST_CLO(double, double, double, double)(ctx, top[-3], top[-2], top[-1], top[0]);
        top -= 4;
        PUSH(x);
        break;
      case OP_clo5:
        ctx = CAST_CTX();
        x = CAST_CLO(double, double, double, double, double)(ctx, top[-4], top[-3], top[-2], top[-1], top[0]);
        top -= 5;
        PUSH(x);
        break;
      case OP_clo6:
        ctx = CAST_CTX();
        x = CAST_CLO(double, double, double, double, double, double)(ctx, top[-5], top[-4], top[-3], top[-2], top[-1], top[0]);
        top -= 6;
        PUSH(x);
        break;
      case OP_clo7:
        ctx = CAST_CTX();
        x = CAST_CLO(double, double, double, double, double, double, double)(ctx, top[-6], top[-5], top[-4], top[-3], top[-2], top[-1], top[0]);
        top -= 7;
        PUSH(x);
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

void ej_print(ej_bytecode *bc) {
  assert(bc);
  
  uint64_t *op = bc->ops;
  while (1) {
    switch (*op) {
      case OP_neg:
        puts("neg");
        break;
      case OP_add:
        puts("add");
        break;
      case OP_sub:
        puts("sub");
        break;
      case OP_mul:
        puts("mul");
        break;
      case OP_div:
        puts("div");
        break;
      case OP_var:
        ++op;
        printf("var %p\n", *(void**)op);
        break;
      case OP_con:
        ++op;
        printf("con %g\n", *(double*)op);
        break;
      case OP_ret:
        puts("ret");
        return;
      
      case OP_fun0: case OP_fun1: case OP_fun2: case OP_fun3:
      case OP_fun4: case OP_fun5: case OP_fun6: case OP_fun7: {
        const uint64_t arity = ARITY(*op);
        void *fun = *(void**)(++op);
        printf("fun %" PRIu64 " %p\n", arity, fun);
        break;
      }
      
      case OP_clo0: case OP_clo1: case OP_clo2: case OP_clo3:
      case OP_clo4: case OP_clo5: case OP_clo6: case OP_clo7: {
        const uint64_t arity = ARITY(*op);
        void *ctx = *(void**)(++op);
        void *fun = *(void**)(++op);
        printf("clo %" PRIu64 " %p ctx %p\n", arity, fun, ctx);
        break;
      }
      
      default:
        puts("(garbage)");
        return;
    }
    ++op;
  }
}

double ej_interp(const char *str) {
  ej_bytecode *bc = ej_compile(str, NULL, 0);
  if (!bc) {
    return NAN;
  }
  const double result = ej_eval(bc);
  ej_free(bc);
  return result;
}
