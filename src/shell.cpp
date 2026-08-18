#include <shell.h>

#include <iostream>

namespace cactus {

int Shell::run() {
  std::cout << "cactus shell 0.1.0\n";
  std::cout << prompt() << std::endl;
  return 0;
}

}  // namespace cactus
