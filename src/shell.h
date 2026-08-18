#ifndef SHELL_H
#define SHELL_H

#include <string>

namespace cactus {

struct Shell {
  Shell() = default;

  int run();

protected:
  std::string prompt() const { return "cactus$ "; }
};

}  // namespace cactus

#endif
