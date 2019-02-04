//
//  expr jit.cpp
//  Expr JIT
//
//  Created by Indi Kernick on 5/2/19.
//  Copyright © 2019 Indi Kernick. All rights reserved.
//

#include "expr jit.hpp"

enum class Op : uint64_t {
  neg,
  add,
  sub,
  mul,
  div,
  mod,
  var,
  con,
  ret,
  
  call0,
  call1,
  call2,
  call3,
  call4,
  call5,
  call6,
  call7
};
