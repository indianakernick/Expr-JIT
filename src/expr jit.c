//
//  expr jit.c
//  Expr JIT
//
//  Created by Indi Kernick on 5/2/19.
//  Copyright © 2019 Indi Kernick. All rights reserved.
//

#include "expr jit.h"

#include <stdlib.h>
#include <stdint.h>

enum {
  OP_neg,
  OP_add,
  OP_sub,
  OP_mul,
  OP_div,
  OP_mod,
  OP_var,
  OP_con,
  OP_ret,
  
  OP_call0,
  OP_call1,
  OP_call2,
  OP_call3,
  OP_call4,
  OP_call5,
  OP_call6,
  OP_call7
};

struct ByteCode {
  uint64_t *ops;
};

ByteCode *ejit_compile(const char *str, Variable *vars, size_t len) {
  return NULL;
}

double ejit_eval(ByteCode *bc) {
  
}

void ejit_free(ByteCode *bc) {
  if (bc) {
    free(bc->ops);
    free(bc);
  }
}
