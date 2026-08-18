#include <string>

#include <gtest/gtest.h>

#include <command.h>
#include <needle.h>

namespace {

cactus::ToolCall run_command(std::string arguments) {
  return cactus::ToolCall{"run_command", std::move(arguments)};
}

TEST(CommandFromToolCall, splits_program_from_arguments) {
  auto command = cactus::command_from_tool_call(
      run_command(R"({"command": "ls -la /tmp"})"));
  ASSERT_TRUE(command.has_value()) << cactus::describe(command.error());
  EXPECT_EQ(command->program, "ls");
  ASSERT_EQ(command->args.size(), 2u);
  EXPECT_EQ(command->args[0], "-la");
  EXPECT_EQ(command->args[1], "/tmp");
}

TEST(CommandFromToolCall, keeps_quoted_arguments_whole) {
  auto command = cactus::command_from_tool_call(
      run_command(R"({"command": "grep 'hello world' notes.txt"})"));
  ASSERT_TRUE(command.has_value()) << cactus::describe(command.error());
  ASSERT_EQ(command->args.size(), 2u);
  EXPECT_EQ(command->args[0], "hello world");
}

TEST(CommandFromToolCall, rejects_malformed_arguments_json) {
  auto command = cactus::command_from_tool_call(run_command("{not json"));
  ASSERT_FALSE(command.has_value());
  EXPECT_EQ(command.error(), cactus::ExecError::BadToolCall);
}

TEST(CommandFromToolCall, rejects_missing_command_key) {
  auto command = cactus::command_from_tool_call(
      run_command(R"({"cmd": "ls"})"));
  ASSERT_FALSE(command.has_value());
  EXPECT_EQ(command.error(), cactus::ExecError::BadToolCall);
}

TEST(CommandFromToolCall, rejects_unterminated_quote) {
  auto command = cactus::command_from_tool_call(
      run_command(R"({"command": "echo 'oops"})"));
  ASSERT_FALSE(command.has_value());
  EXPECT_EQ(command.error(), cactus::ExecError::BadToolCall);
}

TEST(CommandFromToolCall, rejects_blank_command) {
  auto command = cactus::command_from_tool_call(
      run_command(R"({"command": "   "})"));
  ASSERT_FALSE(command.has_value());
  EXPECT_EQ(command.error(), cactus::ExecError::EmptyCommand);
}

TEST(CommandFromToolCall, rejects_an_unknown_tool_name) {
  cactus::ToolCall call{"send_email", R"({"command": "ls"})"};
  auto command = cactus::command_from_tool_call(call);
  ASSERT_FALSE(command.has_value());
  EXPECT_EQ(command.error(), cactus::ExecError::UnknownTool);
}

TEST(IsRisky, flags_destructive_programs) {
  EXPECT_TRUE(cactus::is_risky(cactus::Command{"rm", {"-rf", "/tmp/x"}}));
  EXPECT_TRUE(cactus::is_risky(cactus::Command{"/bin/rm", {"-rf"}}));
  EXPECT_TRUE(cactus::is_risky(cactus::Command{"dd", {}}));
}

TEST(IsRisky, sees_through_sudo) {
  EXPECT_TRUE(cactus::is_risky(cactus::Command{"sudo", {"rm", "-rf", "/"}}));
  EXPECT_TRUE(
      cactus::is_risky(cactus::Command{"sudo", {"-u", "root", "rm", "-rf"}}));
}

TEST(IsRisky, leaves_harmless_programs_alone) {
  EXPECT_FALSE(cactus::is_risky(cactus::Command{"ls", {"-la"}}));
  EXPECT_FALSE(cactus::is_risky(cactus::Command{"grep", {"rm", "notes"}}));
}

TEST(Render, joins_program_and_arguments) {
  EXPECT_EQ(cactus::render(cactus::Command{"ls", {"-la", "/tmp"}}),
            "ls -la /tmp");
}

TEST(Render, quotes_arguments_holding_spaces) {
  EXPECT_EQ(cactus::render(cactus::Command{"grep", {"hello world", "f"}}),
            R"(grep "hello world" f)");
}

TEST(Render, escapes_embedded_quotes) {
  EXPECT_EQ(cactus::render(cactus::Command{"echo", {R"(say "hi")"}}),
            R"(echo "say \"hi\"")");
}

TEST(Execute, reports_a_successful_exit) {
  auto status = cactus::execute(cactus::Command{"true", {}});
  ASSERT_TRUE(status.has_value()) << cactus::describe(status.error());
  EXPECT_EQ(*status, 0);
}

TEST(Execute, reports_a_failing_exit) {
  auto status = cactus::execute(cactus::Command{"false", {}});
  ASSERT_TRUE(status.has_value()) << cactus::describe(status.error());
  EXPECT_EQ(*status, 1);
}

TEST(Execute, runs_a_program_with_arguments) {
  auto status = cactus::execute(cactus::Command{"env", {"true"}});
  ASSERT_TRUE(status.has_value()) << cactus::describe(status.error());
  EXPECT_EQ(*status, 0);
}

TEST(Execute, reports_a_program_that_cannot_start) {
  auto status = cactus::execute(
      cactus::Command{"cactus-no-such-program", {}});
  ASSERT_FALSE(status.has_value());
  EXPECT_EQ(status.error(), cactus::ExecError::StartFailed);
}

TEST(Execute, rejects_an_empty_program) {
  auto status = cactus::execute(cactus::Command{"", {}});
  ASSERT_FALSE(status.has_value());
  EXPECT_EQ(status.error(), cactus::ExecError::EmptyCommand);
}

TEST(ExecErrorTest, every_error_has_a_description) {
  EXPECT_FALSE(cactus::describe(cactus::ExecError::EmptyCommand).empty());
  EXPECT_FALSE(cactus::describe(cactus::ExecError::BadToolCall).empty());
  EXPECT_FALSE(cactus::describe(cactus::ExecError::UnknownTool).empty());
  EXPECT_FALSE(cactus::describe(cactus::ExecError::ForkFailed).empty());
  EXPECT_FALSE(cactus::describe(cactus::ExecError::StartFailed).empty());
}

}  // namespace
