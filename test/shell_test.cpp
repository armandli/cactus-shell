#include <gtest/gtest.h>

#include <shell.h>

TEST(ShellTest, run_returns_success) {
  cactus::Shell shell;
  EXPECT_EQ(shell.run(), 0);
}
