#include <shell.h>

int main(int argc, char** argv) {
  cactus::Shell shell(cactus::ShellConfig::from_args(argc, argv));
  return shell.run();
}
