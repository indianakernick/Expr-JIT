//
//  expr jit.h
//  Expr JIT
//
//  Created by Indi Kernick on 5/2/19.
//  Copyright © 2019 Indi Kernick. All rights reserved.
//

#ifndef expr_jit_h
#define expr_jit_h

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
  EJ_VAR,
  EJ_FUN = 8,
  EJ_FUN0 = EJ_FUN, EJ_FUN1, EJ_FUN2, EJ_FUN3, EJ_FUN4, EJ_FUN5, EJ_FUN6, EJ_FUN7,
  EJ_CLO = 16,
  EJ_CLO0 = EJ_CLO, EJ_CLO1, EJ_CLO2, EJ_CLO3, EJ_CLO4, EJ_CLO5, EJ_CLO6, EJ_CLO7
};

typedef struct Variable {
  const char *name;
  double *addr;
  int type;
  void *ctx;
} Variable;

typedef struct ByteCode ByteCode;

ByteCode *ejit_compile(const char *, Variable *, size_t);
double ejit_eval(ByteCode *);
void ejit_free(ByteCode *);

#ifdef __cplusplus
}
#endif

#endif
