//
//  main.cpp
//  Test
//
//  Created by Indi Kernick on 5/2/19.
//  Copyright © 2019 Indi Kernick. All rights reserved.
//

#include <iostream>

#include "expr jit.hpp"

int main() {
  double yeah = 12.0;
  Variable vars[] = {
    {"yeah", &yeah}
  };
  std::unique_ptr<ByteCode> bc = compile("yeah", vars);
  std::cout << yeah << ' ' << interpret(bc.get()) << '\n';
}
