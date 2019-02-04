//
//  expr jit.hpp
//  Expr JIT
//
//  Created by Indi Kernick on 5/2/19.
//  Copyright © 2019 Indi Kernick. All rights reserved.
//

#ifndef expr_jit_hpp
#define expr_jit_hpp

#include <memory>
#include <vector>
#include <string_view>

template <typename Elem>
struct span {
  using value_type = const Elem;

  span()
    : dat{nullptr}, len{0} {}

  template <size_t Size>
  span(value_type(&array)[Size])
    : dat{array}, len{Size} {}
  
  span(value_type *begin, value_type *end)
    : dat{begin}, len{end - begin} {}
  span(value_type *data, size_t size)
    : dat{data}, len{size} {}
  
  value_type *begin() const {
    return dat;
  }
  value_type *end() const {
    return dat + len;
  }
  
private:
  value_type *dat;
  size_t len;
};

struct Variable {
  std::string_view name;
  double *addr;
};

struct Function {
  std::string_view name;
  void *addr;
  size_t arity;
};

struct ByteCode;

std::unique_ptr<ByteCode> compile(std::string_view, span<Variable> = {}, span<Function> = {});
double interpret(ByteCode *);

#endif
