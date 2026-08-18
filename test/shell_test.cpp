#include <cstdlib>

#include <filesystem>
#include <sstream>
#include <string>

#include <gtest/gtest.h>

#include <shell.h>

namespace {

// Drives the REPL over strings. Every case here stays clear of the model: a
// Shell with no model path reports that and keeps going, so the loop, the
// builtins, and EOF handling are all testable without weights.
std::string feed(const std::string& input) {
  std::istringstream in(input);
  std::ostringstream out;
  cactus::Shell shell;
  EXPECT_EQ(shell.run(in, out), 0);
  return out.str();
}

struct ShellDirectory : public ::testing::Test {
protected:
  void SetUp() override { start_ = std::filesystem::current_path(); }
  void TearDown() override { std::filesystem::current_path(start_); }

  std::filesystem::path start_;
};

TEST(ShellTest, writes_a_prompt) {
  EXPECT_NE(feed("exit\n").find("cactus$ "), std::string::npos);
}

TEST(ShellTest, exit_ends_the_loop) {
  std::istringstream in("exit\nthis line is never read\n");
  std::ostringstream out;
  cactus::Shell shell;
  EXPECT_EQ(shell.run(in, out), 0);

  std::string leftover;
  ASSERT_TRUE(std::getline(in, leftover));
  EXPECT_EQ(leftover, "this line is never read");
}

TEST(ShellTest, quit_ends_the_loop) {
  std::istringstream in("quit\nthis line is never read\n");
  std::ostringstream out;
  cactus::Shell shell;
  EXPECT_EQ(shell.run(in, out), 0);

  std::string leftover;
  ASSERT_TRUE(std::getline(in, leftover));
  EXPECT_EQ(leftover, "this line is never read");
}

TEST(ShellTest, end_of_input_ends_the_loop) {
  EXPECT_NE(feed("").find("cactus$ "), std::string::npos);
}

TEST(ShellTest, blank_lines_never_reach_the_model) {
  EXPECT_EQ(feed("\n   \n\t\nexit\n").find("no model"), std::string::npos);
}

TEST(ShellTest, surrounding_whitespace_does_not_hide_a_builtin) {
  EXPECT_EQ(feed("   exit   \n").find("no model"), std::string::npos);
}

TEST(ShellTest, a_request_without_a_model_reports_and_continues) {
  std::string output = feed("list the files here\nexit\n");
  EXPECT_NE(output.find("no model"), std::string::npos);
  EXPECT_NE(output.find("cactus$ "), output.rfind("cactus$ "));
}

TEST_F(ShellDirectory, cd_changes_the_working_directory) {
  std::filesystem::path expected =
      std::filesystem::canonical(std::filesystem::temp_directory_path());
  feed("cd " + expected.string() + "\nexit\n");
  EXPECT_EQ(std::filesystem::canonical(std::filesystem::current_path()),
            expected);
}

TEST_F(ShellDirectory, cd_accepts_a_quoted_directory) {
  std::filesystem::path expected =
      std::filesystem::canonical(std::filesystem::temp_directory_path());
  feed("cd \"" + expected.string() + "\"\nexit\n");
  EXPECT_EQ(std::filesystem::canonical(std::filesystem::current_path()),
            expected);
}

TEST_F(ShellDirectory, a_bad_cd_reports_without_quitting) {
  std::string output = feed("cd /no/such/directory\nexit\n");
  EXPECT_NE(output.find("cd: cannot change to"), std::string::npos);
  EXPECT_EQ(std::filesystem::current_path(), start_);
}

TEST_F(ShellDirectory, cd_with_two_directories_is_rejected) {
  std::string output = feed("cd /tmp /usr\nexit\n");
  EXPECT_NE(output.find("exactly one directory"), std::string::npos);
  EXPECT_EQ(std::filesystem::current_path(), start_);
}

TEST_F(ShellDirectory, bare_cd_goes_home) {
  const char* home = std::getenv("HOME");
  if (home == nullptr)
    GTEST_SKIP() << "HOME is not set";
  feed("cd\nexit\n");
  EXPECT_EQ(std::filesystem::current_path(), std::filesystem::path(home));
}

TEST(ShellConfigTest, from_args_prefers_the_first_argument) {
  char program[] = "cactus";
  char path[] = "/weights/needle";
  char* argv[] = {program, path, nullptr};
  EXPECT_EQ(cactus::ShellConfig::from_args(2, argv).model_path,
            "/weights/needle");
}

TEST(ShellConfigTest, from_args_falls_back_to_the_environment) {
  char program[] = "cactus";
  char* argv[] = {program, nullptr};
  const char* from_env = std::getenv("CACTUS_NEEDLE_MODEL");
  std::string expected = from_env == nullptr ? std::string() : from_env;
  EXPECT_EQ(cactus::ShellConfig::from_args(1, argv).model_path, expected);
}

}  // namespace
