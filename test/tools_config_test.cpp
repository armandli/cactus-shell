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

  cactus::ToolCatalog mCatalog;
};

TEST_F(ShippedCatalog, holds_every_tool) {
  EXPECT_EQ(mCatalog.specs().size(), 26u);
}

TEST_F(ShippedCatalog, names_the_tools_the_shell_advertises) {
  for (std::string_view name :
       {"ls", "pwd", "cd", "cat", "head", "tail", "grep", "find", "wc",
        "which", "file", "du", "df", "ps", "date", "whoami", "echo", "sort",
        "mkdir", "rmdir", "rm", "mv", "cp", "touch", "chmod", "kill"})
    EXPECT_NE(mCatalog.find(name), nullptr) << name;
}

TEST_F(ShippedCatalog, destructive_tools_are_marked_risky) {
  for (std::string_view name :
       {"rm", "rmdir", "mv", "cp", "chmod", "kill", "mkdir", "touch"}) {
    const cactus::ToolSpec* spec = mCatalog.find(name);
    ASSERT_NE(spec, nullptr) << name;
    EXPECT_TRUE(spec->risky) << name;
  }
}

TEST_F(ShippedCatalog, read_only_tools_are_not_risky) {
  for (std::string_view name :
       {"ls", "pwd", "cd", "cat", "head", "grep", "find", "which", "df"}) {
    const cactus::ToolSpec* spec = mCatalog.find(name);
    ASSERT_NE(spec, nullptr) << name;
    EXPECT_FALSE(spec->risky) << name;
  }
}

TEST_F(ShippedCatalog, cd_is_the_only_in_process_tool) {
  for (const cactus::ToolSpec& spec : mCatalog.specs())
    EXPECT_TRUE(not spec.in_process or spec.name == "cd") << spec.name;
  EXPECT_TRUE(mCatalog.find("cd")->in_process);
}

// find wants its path before -name, and mv its sources before the destination,
// so a transcription that reordered parameters would build a broken argv.
TEST_F(ShippedCatalog, keeps_parameters_in_the_order_the_program_expects) {
  const cactus::ToolSpec* find = mCatalog.find("find");
  ASSERT_NE(find, nullptr);
  ASSERT_EQ(find->params.size(), 2u);
  EXPECT_EQ(find->params[0].name, "path");
  EXPECT_EQ(find->params[1].name, "name");

  const cactus::ToolSpec* move = mCatalog.find("mv");
  ASSERT_NE(move, nullptr);
  ASSERT_EQ(move->params.size(), 2u);
  EXPECT_EQ(move->params[0].name, "sources");
  EXPECT_EQ(move->params[1].name, "destination");
}

TEST_F(ShippedCatalog, spells_the_flags_the_programs_actually_take) {
  const cactus::ToolSpec* ls = mCatalog.find("ls");
  ASSERT_NE(ls, nullptr);
  ASSERT_EQ(ls->params.size(), 4u);
  EXPECT_EQ(ls->params[0].flag, "-l");
  EXPECT_EQ(ls->params[1].flag, "-a");
  EXPECT_EQ(ls->params[2].flag, "-R");
  EXPECT_TRUE(ls->params[3].flag.empty());

  const cactus::ToolSpec* head = mCatalog.find("head");
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
  for (std::string_view name : {"sudo", "sh", "bash", "run_command", "eval"})
    EXPECT_EQ(mCatalog.find(name), nullptr) << name;
}

}  // namespace
