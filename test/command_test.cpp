#include <expected>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include <command.h>
#include <needle.h>
#include <tools.h>

namespace {

// The shipped config is the catalog these cases are written against: the
// tools for ls, head, find, mv, cat, rm, cd and pwd all come from it, with the
// flag spellings and parameter order the assertions below expect. Calls name
// the tool as the model sees it, which is not the program it runs.
const cactus::ToolCatalog& catalog() {
  static const cactus::ToolCatalog loaded =
      cactus::ToolCatalog::load(CACTUS_DEFAULT_TOOL_CONFIG)
          .value_or(cactus::ToolCatalog());
  return loaded;
}

std::expected<cactus::Command, cactus::ExecError> from_call(
    std::string_view name,
    std::string arguments)
{
  return cactus::command_from_tool_call(
      cactus::ToolCall{std::string(name), std::move(arguments)}, catalog());
}

TEST(CommandCatalog, the_shipped_config_loads) {
  EXPECT_FALSE(catalog().empty());
}

// Flags land in declaration order and a false flag emits nothing, so the model
// setting `all` without `long` must not shift the path out of position.
TEST(CommandFromToolCall, emits_flags_then_the_optional_positional) {
  auto command =
      from_call("list_files", R"({"long": true, "all": true, "path": "/tmp"})");
  ASSERT_TRUE(command.has_value()) << cactus::describe(command.error());
  EXPECT_EQ(command->program, "ls");
  ASSERT_EQ(command->args.size(), 3u);
  EXPECT_EQ(command->args[0], "-l");
  EXPECT_EQ(command->args[1], "-a");
  EXPECT_EQ(command->args[2], "/tmp");
}

TEST(CommandFromToolCall, drops_flags_the_model_set_to_false) {
  auto command = from_call("list_files", R"({"long": false, "all": true})");
  ASSERT_TRUE(command.has_value()) << cactus::describe(command.error());
  ASSERT_EQ(command->args.size(), 1u);
  EXPECT_EQ(command->args[0], "-a");
}

TEST(CommandFromToolCall, skips_an_absent_optional_parameter) {
  auto command = from_call("list_files", "{}");
  ASSERT_TRUE(command.has_value()) << cactus::describe(command.error());
  EXPECT_EQ(command->program, "ls");
  EXPECT_TRUE(command->args.empty());
}

TEST(CommandFromToolCall, spells_a_number_out_after_its_flag) {
  auto command = from_call("print_first_lines", R"({"lines": 20, "path": "README.md"})");
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
  auto command = from_call("find_files_by_name", R"({"path": "src", "name": "*.cpp"})");
  ASSERT_TRUE(command.has_value()) << cactus::describe(command.error());
  ASSERT_EQ(command->args.size(), 3u);
  EXPECT_EQ(command->args[0], "src");
  EXPECT_EQ(command->args[1], "-name");
  EXPECT_EQ(command->args[2], "*.cpp");
}

TEST(CommandFromToolCall, expands_a_list_before_the_positional_after_it) {
  auto command =
      from_call("move_or_rename", R"({"sources": ["a", "b"], "destination": "dest"})");
  ASSERT_TRUE(command.has_value()) << cactus::describe(command.error());
  ASSERT_EQ(command->args.size(), 3u);
  EXPECT_EQ(command->args[0], "a");
  EXPECT_EQ(command->args[1], "b");
  EXPECT_EQ(command->args[2], "dest");
}

// Each JSON value becomes exactly one argv entry, so a path holding a space
// survives without any quoting or splitting step.
TEST(CommandFromToolCall, keeps_one_argv_entry_per_list_item) {
  auto command = from_call("print_file", R"({"paths": ["notes.txt", "my file.txt"]})");
  ASSERT_TRUE(command.has_value()) << cactus::describe(command.error());
  ASSERT_EQ(command->args.size(), 2u);
  EXPECT_EQ(command->args[1], "my file.txt");
}

TEST(CommandFromToolCall, accepts_a_tool_that_takes_no_parameters) {
  auto command = from_call("print_working_directory", "{}");
  ASSERT_TRUE(command.has_value()) << cactus::describe(command.error());
  EXPECT_EQ(command->program, "pwd");
  EXPECT_TRUE(command->args.empty());
}

TEST(CommandFromToolCall, carries_the_risky_mark_off_the_catalog) {
  auto risky = from_call("delete_files", R"({"recursive": true, "paths": ["tmp"]})");
  ASSERT_TRUE(risky.has_value()) << cactus::describe(risky.error());
  EXPECT_TRUE(risky->risky);

  auto harmless = from_call("list_files", "{}");
  ASSERT_TRUE(harmless.has_value()) << cactus::describe(harmless.error());
  EXPECT_FALSE(harmless->risky);
}

TEST(CommandFromToolCall, marks_cd_as_in_process) {
  auto command = from_call("change_directory", R"({"path": "/tmp"})");
  ASSERT_TRUE(command.has_value()) << cactus::describe(command.error());
  EXPECT_TRUE(command->in_process);

  auto other = from_call("print_working_directory", "{}");
  ASSERT_TRUE(other.has_value()) << cactus::describe(other.error());
  EXPECT_FALSE(other->in_process);
}

// Fail-closed: a name the catalog does not carry runs nothing at all, which is
// what lets the shell drop the old denylist and its sudo look-through.
TEST(CommandFromToolCall, rejects_a_tool_outside_the_catalog) {
  auto command = from_call("run_command", R"({"command": "rm -rf /"})");
  ASSERT_FALSE(command.has_value());
  EXPECT_EQ(command.error(), cactus::ExecError::UnknownTool);
}

TEST(CommandFromToolCall, rejects_sudo) {
  auto command = from_call("sudo", R"({"paths": ["x"]})");
  ASSERT_FALSE(command.has_value());
  EXPECT_EQ(command.error(), cactus::ExecError::UnknownTool);
}

TEST(CommandFromToolCall, rejects_a_missing_required_parameter) {
  auto command = from_call("move_or_rename", R"({"sources": ["a"]})");
  ASSERT_FALSE(command.has_value());
  EXPECT_EQ(command.error(), cactus::ExecError::MissingArgument);
}

TEST(CommandFromToolCall, rejects_a_parameter_of_the_wrong_type) {
  auto flag = from_call("list_files", R"({"all": "yes"})");
  ASSERT_FALSE(flag.has_value());
  EXPECT_EQ(flag.error(), cactus::ExecError::BadToolCall);

  auto number = from_call("print_first_lines", R"({"lines": "20", "path": "f"})");
  ASSERT_FALSE(number.has_value());
  EXPECT_EQ(number.error(), cactus::ExecError::BadToolCall);

  auto list = from_call("print_file", R"({"paths": "notes.txt"})");
  ASSERT_FALSE(list.has_value());
  EXPECT_EQ(list.error(), cactus::ExecError::BadToolCall);
}

TEST(CommandFromToolCall, rejects_a_list_holding_a_non_string) {
  auto command = from_call("print_file", R"({"paths": ["notes.txt", 7]})");
  ASSERT_FALSE(command.has_value());
  EXPECT_EQ(command.error(), cactus::ExecError::BadToolCall);
}

TEST(CommandFromToolCall, rejects_malformed_arguments_json) {
  auto command = from_call("list_files", "{not json");
  ASSERT_FALSE(command.has_value());
  EXPECT_EQ(command.error(), cactus::ExecError::BadToolCall);
}

TEST(CommandFromToolCall, rejects_arguments_that_are_not_an_object) {
  auto command = from_call("list_files", R"(["-a"])");
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
