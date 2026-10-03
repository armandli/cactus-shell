#include <algorithm>
#include <regex>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include <json_util.h>
#include <tools.h>

namespace {

// The shipped catalog moved out of C++ and into JSON, so nothing but a test
// against the real file stops the two from drifting apart.
struct ShippedCatalog : public ::testing::Test {
protected:
  void SetUp() override {
    auto loaded = cactus::ToolCatalog::load(CACTUS_DEFAULT_TOOL_CONFIG);
    if (not loaded.has_value())
      FAIL() << cactus::describe(loaded.error());
    mCatalog = std::move(*loaded);
  }

  // Tools are named for the model ("delete_files"), but what makes one risky
  // or in-process is the program it runs, so these cases look tools up that
  // way.
  const cactus::ToolSpec* by_program(std::string_view program) const {
    auto specs = mCatalog.specs();
    auto found = std::ranges::find(specs, program, &cactus::ToolSpec::program);
    return found == specs.end() ? nullptr : &*found;
  }

  cactus::ToolCatalog mCatalog;
};

TEST_F(ShippedCatalog, holds_every_tool) {
  EXPECT_EQ(mCatalog.specs().size(), 26u);
}

TEST_F(ShippedCatalog, runs_the_programs_the_shell_advertises) {
  for (std::string_view program :
       {"ls", "pwd", "cd", "cat", "head", "tail", "grep", "find", "wc",
        "which", "file", "du", "df", "ps", "date", "whoami", "echo", "sort",
        "mkdir", "rmdir", "rm", "mv", "cp", "touch", "chmod", "kill"})
    EXPECT_NE(by_program(program), nullptr) << program;
}

// Needle 3 picks tools by what their names say, and does far worse with bare
// program names like "rm" or "wc" than with ones a user would say.
TEST_F(ShippedCatalog, names_tools_for_the_model_not_the_program) {
  for (const cactus::ToolSpec& spec : mCatalog.specs())
    EXPECT_NE(spec.name, spec.program) << spec.name;
}

// With more tools than the model sees at once, a tool no trigger reaches is
// left to retrieval, which misses most shell requests.
TEST_F(ShippedCatalog, every_tool_declares_triggers_that_compile) {
  for (const cactus::ToolSpec& spec : mCatalog.specs()) {
    EXPECT_FALSE(spec.triggers.empty()) << spec.name;
    for (const std::string& trigger : spec.triggers)
      EXPECT_NO_THROW(std::regex(trigger, std::regex::icase))
          << spec.name << ": " << trigger;
  }
}

TEST_F(ShippedCatalog, destructive_tools_are_marked_risky) {
  for (std::string_view program :
       {"rm", "rmdir", "mv", "cp", "chmod", "kill", "mkdir", "touch"}) {
    const cactus::ToolSpec* spec = by_program(program);
    ASSERT_NE(spec, nullptr) << program;
    EXPECT_TRUE(spec->risky) << program;
  }
}

TEST_F(ShippedCatalog, read_only_tools_are_not_risky) {
  for (std::string_view program :
       {"ls", "pwd", "cd", "cat", "head", "grep", "find", "which", "df"}) {
    const cactus::ToolSpec* spec = by_program(program);
    ASSERT_NE(spec, nullptr) << program;
    EXPECT_FALSE(spec->risky) << program;
  }
}

TEST_F(ShippedCatalog, cd_is_the_only_in_process_tool) {
  for (const cactus::ToolSpec& spec : mCatalog.specs())
    EXPECT_TRUE(not spec.in_process or spec.program == "cd") << spec.name;
  ASSERT_NE(by_program("cd"), nullptr);
  EXPECT_TRUE(by_program("cd")->in_process);
}

// find wants its path before -name, and mv its sources before the destination,
// so a transcription that reordered parameters would build a broken argv.
TEST_F(ShippedCatalog, keeps_parameters_in_the_order_the_program_expects) {
  const cactus::ToolSpec* find = by_program("find");
  ASSERT_NE(find, nullptr);
  ASSERT_EQ(find->params.size(), 2u);
  EXPECT_EQ(find->params[0].name, "path");
  EXPECT_EQ(find->params[1].name, "name");

  const cactus::ToolSpec* move = by_program("mv");
  ASSERT_NE(move, nullptr);
  ASSERT_EQ(move->params.size(), 2u);
  EXPECT_EQ(move->params[0].name, "sources");
  EXPECT_EQ(move->params[1].name, "destination");
}

TEST_F(ShippedCatalog, spells_the_flags_the_programs_actually_take) {
  const cactus::ToolSpec* ls = by_program("ls");
  ASSERT_NE(ls, nullptr);
  ASSERT_EQ(ls->params.size(), 4u);
  EXPECT_EQ(ls->params[0].flag, "-l");
  EXPECT_EQ(ls->params[1].flag, "-a");
  EXPECT_EQ(ls->params[2].flag, "-R");
  EXPECT_TRUE(ls->params[3].flag.empty());

  const cactus::ToolSpec* head = by_program("head");
  ASSERT_NE(head, nullptr);
  EXPECT_EQ(head->params[0].kind, cactus::ParamKind::Number);
  EXPECT_EQ(head->params[0].flag, "-n");
}

TEST_F(ShippedCatalog, renders_a_schema_the_model_can_read) {
  auto doc = cactus::JsonDoc::parse(mCatalog.schema_json());
  ASSERT_TRUE(doc.has_value()) << cactus::describe(doc.error());
  simdjson::dom::array entries;
  ASSERT_FALSE(doc->root().get(entries));
  EXPECT_EQ(entries.size(), mCatalog.specs().size());
}

TEST_F(ShippedCatalog, carries_no_escape_hatch) {
  for (std::string_view name : {"sudo", "sh", "bash", "run_command", "eval"}) {
    EXPECT_EQ(mCatalog.find(name), nullptr) << name;
    EXPECT_EQ(by_program(name), nullptr) << name;
  }
}

}  // namespace
