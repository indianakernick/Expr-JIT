//
//  main.c
//  Test
//
//  Created by Indi Kernick on 5/2/19.
//  Copyright © 2019 Indi Kernick. All rights reserved.
//

#include "expr jit.h"

#include <stdio.h>

static double three(double *ctx) {
  return *ctx;
}

static double calculate(double a) {
  return ( 1/(-a+1) - sqrt(2/(a+2)) + 3/(a+3) );
}

int main() {
  double a = 7;
  double ctx = 3;
  ej_variable vars[] = {
    {"a", &a, EJ_VAR},
    {"three", &three, EJ_CLO0, &ctx}
  };
  const char *expr = "( 1/(-a+1) - sqrt(2/(a+2)) + three()/(a+three()) )";
  ej_bytecode *bc = ej_compile(expr, vars, sizeof(vars)/sizeof(vars[0]));
  printf("a %p %g\n", &a, a);
  ej_print(bc);
  double result = ej_eval(bc);
  printf("Eval: %g\nReal: %g\n", result, calculate(a));
  ej_free(bc);
}
