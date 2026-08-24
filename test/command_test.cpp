#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include <command.h>
#include <needle.h>

namespace {

cactus::ToolCall call_of(std::string_view name, std::string arguments) {
  return cactus::ToolCall{std::string(name), std::move(arguments)};
}

// Flags land in declaration order and a false flag emits nothing, so the model
// setting `all` without `long` must not shift the path out of position.
TEST(CommandFromToolCall, emits_flags_then_the_optional_positional) {
  auto command = cactus::command_from_tool_call(
      call_of("ls", R"({"long": true, "all": true, "path": "/tmp"})"));
  ASSERT_TRUE(command.has_value()) << cactus::describe(command.error());
  EXPECT_EQ(command->program, "ls");
  ASSERT_EQ(command->args.size(), 3u);
  EXPECT_EQ(command->args[0], "-l");
  EXPECT_EQ(command->args[1], "-a");
  EXPECT_EQ(command->args[2], "/tmp");
}

TEST(CommandFromToolCall, drops_flags_the_model_set_to_false) {
  auto command = cactus::command_from_tool_call(
      call_of("ls", R"({"long": false, "all": true})"));
  ASSERT_TRUE(command.has_value()) << cactus::describe(command.error());
  ASSERT_EQ(command->args.size(), 1u);
  EXPECT_EQ(command->args[0], "-a");
}

TEST(CommandFromToolCall, skips_an_absent_optional_parameter) {
  auto command = cactus::command_from_tool_call(call_of("ls", "{}"));
  ASSERT_TRUE(command.has_value()) << cactus::describe(command.error());
  EXPECT_EQ(command->program, "ls");
  EXPECT_TRUE(command->args.empty());
}

TEST(CommandFromToolCall, spells_a_number_out_after_its_flag) {
  auto command = cactus::command_from_tool_call(
      call_of("head", R"({"lines": 20, "path": "README.md"})"));
  ASSERT_TRUE(command.has_value()) << cactus::describe(command.error());
  EXPECT_EQ(command->program, "head");
  ASSERT_EQ(command->args.size(), 3u);
  EXPECT_EQ(command->args[0], "-n");
  EXPECT_EQ(command->args[1], "20");
  EXPECT_EQ(command->args[2], "README.md");
}

// find wants its path before -name, which is why parameters are emitted in
// declaration order rather than flags-first.
TEST(CommandFromToolCall, emits_an_option_after_the_positional_it_follows) {
  auto command = cactus::command_from_tool_call(
      call_of("find", R"({"path": "src", "name": "*.cpp"})"));
  ASSERT_TRUE(command.has_value()) << cactus::describe(command.error());
  ASSERT_EQ(command->args.size(), 3u);
  EXPECT_EQ(command->args[0], "src");
  EXPECT_EQ(command->args[1], "-name");
  EXPECT_EQ(command->args[2], "*.cpp");
}

TEST(CommandFromToolCall, expands_a_list_before_the_positional_after_it) {
  auto command = cactus::command_from_tool_call(
      call_of("mv", R"({"sources": ["a", "b"], "destination": "dest"})"));
  ASSERT_TRUE(command.has_value()) << cactus::describe(command.error());
  ASSERT_EQ(command->args.size(), 3u);
  EXPECT_EQ(command->args[0], "a");
  EXPECT_EQ(command->args[1], "b");
  EXPECT_EQ(command->args[2], "dest");
}

// Each JSON value becomes exactly one argv entry, so a path holding a space
// survives without any quoting or splitting step.
TEST(CommandFromToolCall, keeps_one_argv_entry_per_list_item) {
  auto command = cactus::command_from_tool_call(
      call_of("cat", R"({"paths": ["notes.txt", "my file.txt"]})"));
  ASSERT_TRUE(command.has_value()) << cactus::describe(command.error());
  ASSERT_EQ(command->args.size(), 2u);
  EXPECT_EQ(command->args[1], "my file.txt");
}

TEST(CommandFromToolCall, accepts_a_tool_that_takes_no_parameters) {
  auto command = cactus::command_from_tool_call(call_of("pwd", "{}"));
  ASSERT_TRUE(command.has_value()) << cactus::describe(command.error());
  EXPECT_EQ(command->program, "pwd");
  EXPECT_TRUE(command->args.empty());
}

TEST(CommandFromToolCall, carries_the_risky_mark_off_the_catalog) {
  auto risky = cactus::command_from_tool_call(
      call_of("rm", R"({"recursive": true, "paths": ["tmp"]})"));
  ASSERT_TRUE(risky.has_value()) << cactus::describe(risky.error());
  EXPECT_TRUE(risky->risky);

  auto harmless = cactus::command_from_tool_call(call_of("ls", "{}"));
  ASSERT_TRUE(harmless.has_value()) << cactus::describe(harmless.error());
  EXPECT_FALSE(harmless->risky);
}

TEST(CommandFromToolCall, marks_cd_as_in_process) {
  auto command =
      cactus::command_from_tool_call(call_of("cd", R"({"path": "/tmp"})"));
  ASSERT_TRUE(command.has_value()) << cactus::describe(command.error());
  EXPECT_TRUE(command->in_process);

  auto other = cactus::command_from_tool_call(call_of("pwd", "{}"));
  ASSERT_TRUE(other.has_value()) << cactus::describe(other.error());
  EXPECT_FALSE(other->in_process);
}

// Fail-closed: a name the catalog does not carry runs nothing at all, which is
// what lets the shell drop the old denylist and its sudo look-through.
TEST(CommandFromToolCall, rejects_a_tool_outside_the_catalog) {
  auto command = cactus::command_from_tool_call(
      call_of("run_command", R"({"command": "rm -rf /"})"));
  ASSERT_FALSE(command.has_value());
  EXPECT_EQ(command.error(), cactus::ExecError::UnknownTool);
}

TEST(CommandFromToolCall, rejects_sudo) {
  auto command =
      cactus::command_from_tool_call(call_of("sudo", R"({"paths": ["x"]})"));
  ASSERT_FALSE(command.has_value());
  EXPECT_EQ(command.error(), cactus::ExecError::UnknownTool);
}

TEST(CommandFromToolCall, rejects_a_missing_required_parameter) {
  auto command = cactus::command_from_tool_call(
      call_of("mv", R"({"sources": ["a"]})"));
  ASSERT_FALSE(command.has_value());
  EXPECT_EQ(command.error(), cactus::ExecError::MissingArgument);
}

TEST(CommandFromToolCall, rejects_a_parameter_of_the_wrong_type) {
  auto flag = cactus::command_from_tool_call(
      call_of("ls", R"({"all": "yes"})"));
  ASSERT_FALSE(flag.has_value());
  EXPECT_EQ(flag.error(), cactus::ExecError::BadToolCall);

  auto number = cactus::command_from_tool_call(
      call_of("head", R"({"lines": "20", "path": "f"})"));
  ASSERT_FALSE(number.has_value());
  EXPECT_EQ(number.error(), cactus::ExecError::BadToolCall);

  auto list = cactus::command_from_tool_call(
      call_of("cat", R"({"paths": "notes.txt"})"));
  ASSERT_FALSE(list.has_value());
  EXPECT_EQ(list.error(), cactus::ExecError::BadToolCall);
}

TEST(CommandFromToolCall, rejects_a_list_holding_a_non_string) {
  auto command = cactus::command_from_tool_call(
      call_of("cat", R"({"paths": ["notes.txt", 7]})"));
  ASSERT_FALSE(command.has_value());
  EXPECT_EQ(command.error(), cactus::ExecError::BadToolCall);
}

TEST(CommandFromToolCall, rejects_malformed_arguments_json) {
  auto command = cactus::command_from_tool_call(call_of("ls", "{not json"));
  ASSERT_FALSE(command.has_value());
  EXPECT_EQ(command.error(), cactus::ExecError::BadToolCall);
}

TEST(CommandFromToolCall, rejects_arguments_that_are_not_an_object) {
  auto command = cactus::command_from_tool_call(call_of("ls", R"(["-a"])"));
  ASSERT_FALSE(command.has_value());
  EXPECT_EQ(command.error(), cactus::ExecError::BadToolCall);
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
  EXPECT_FALSE(cactus::describe(cactus::ExecError::MissingArgument).empty());
  EXPECT_FALSE(cactus::describe(cactus::ExecError::UnknownTool).empty());
  EXPECT_FALSE(cactus::describe(cactus::ExecError::ForkFailed).empty());
  EXPECT_FALSE(cactus::describe(cactus::ExecError::StartFailed).empty());
}

}  // namespace
