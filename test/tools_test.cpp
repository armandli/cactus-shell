#include <set>
#include <span>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include <json_util.h>
#include <tools.h>

namespace {

namespace sj = simdjson;

std::span<const cactus::ToolSpec> catalog() {
  return cactus::tool_catalog();
}

// Parses the rendered catalog once and hands back the array of tool entries,
// in the same order as the table they came from.
struct CatalogJson : public ::testing::Test {
protected:
  void SetUp() override {
    auto parsed = cactus::JsonDoc::parse(cactus::tool_catalog_json());
    ASSERT_TRUE(parsed.has_value()) << cactus::describe(parsed.error());
    doc = std::move(*parsed);
    ASSERT_FALSE(doc.root().get(entries));
  }

  // The "function" object of the entry at `index`, as an element.
  sj::dom::element function_of(std::size_t index) {
    sj::dom::element entry = entries.at(index);
    auto found = entry.at_key("function");
    EXPECT_FALSE(found.error());
    return found.value();
  }

  cactus::JsonDoc doc;
  sj::dom::array entries;
};

TEST(ToolCatalog, is_not_empty) {
  EXPECT_FALSE(catalog().empty());
}

TEST(ToolCatalog, names_are_unique) {
  std::set<std::string_view> seen;
  for (const cactus::ToolSpec& spec : catalog())
    EXPECT_TRUE(seen.insert(spec.name).second)
        << "duplicate tool name: " << spec.name;
}

TEST(ToolCatalog, every_spec_is_described) {
  for (const cactus::ToolSpec& spec : catalog()) {
    EXPECT_FALSE(spec.name.empty());
    EXPECT_FALSE(spec.program.empty());
    EXPECT_FALSE(spec.description.empty()) << spec.name;
  }
}

// The whole point of the catalog is that the model reads these descriptions
// instead of recalling flag spellings, so an undescribed parameter is a bug.
TEST(ToolCatalog, every_parameter_is_described) {
  for (const cactus::ToolSpec& spec : catalog())
    for (const cactus::ToolParam& param : spec.params) {
      EXPECT_FALSE(param.name.empty()) << spec.name;
      EXPECT_FALSE(param.description.empty()) << spec.name << "." << param.name;
    }
}

// The argv builder emits param.flag for these kinds and nothing but the value
// for the others, so a mismatch here would silently produce a broken argv.
TEST(ToolCatalog, only_flagged_kinds_carry_a_flag) {
  for (const cactus::ToolSpec& spec : catalog())
    for (const cactus::ToolParam& param : spec.params) {
      bool flagged = param.kind == cactus::ParamKind::Flag or
                     param.kind == cactus::ParamKind::Option or
                     param.kind == cactus::ParamKind::Number;
      EXPECT_EQ(flagged, not param.flag.empty())
          << spec.name << "." << param.name;
    }
}

TEST(ToolCatalog, parameter_names_are_unique_within_a_tool) {
  for (const cactus::ToolSpec& spec : catalog()) {
    std::set<std::string_view> seen;
    for (const cactus::ToolParam& param : spec.params)
      EXPECT_TRUE(seen.insert(param.name).second)
          << spec.name << " repeats " << param.name;
  }
}

TEST(ToolCatalog, cd_is_the_only_in_process_tool) {
  for (const cactus::ToolSpec& spec : catalog())
    EXPECT_TRUE(not spec.in_process or spec.name == "cd") << spec.name;
}

TEST(ToolCatalog, destructive_tools_are_marked_risky) {
  for (std::string_view name :
       {"rm", "rmdir", "mv", "cp", "chmod", "kill", "mkdir", "touch"}) {
    const cactus::ToolSpec* spec = cactus::find_tool(name);
    ASSERT_NE(spec, nullptr) << name;
    EXPECT_TRUE(spec->risky) << name;
  }
}

TEST(ToolCatalog, read_only_tools_are_not_risky) {
  for (std::string_view name :
       {"ls", "pwd", "cd", "cat", "head", "grep", "find", "which", "df"}) {
    const cactus::ToolSpec* spec = cactus::find_tool(name);
    ASSERT_NE(spec, nullptr) << name;
    EXPECT_FALSE(spec->risky) << name;
  }
}

TEST(FindTool, finds_a_catalog_entry) {
  const cactus::ToolSpec* spec = cactus::find_tool("ls");
  ASSERT_NE(spec, nullptr);
  EXPECT_EQ(spec->program, "ls");
}

// Fail-closed: anything the table does not name is unreachable, which is what
// retires the old denylist and its sudo look-through.
TEST(FindTool, rejects_a_name_outside_the_catalog) {
  EXPECT_EQ(cactus::find_tool("tar"), nullptr);
  EXPECT_EQ(cactus::find_tool("sudo"), nullptr);
  EXPECT_EQ(cactus::find_tool("run_command"), nullptr);
  EXPECT_EQ(cactus::find_tool(""), nullptr);
}

TEST_F(CatalogJson, is_an_array_with_one_entry_per_spec) {
  EXPECT_TRUE(doc.root().is_array());
  EXPECT_EQ(entries.size(), catalog().size());
}

TEST_F(CatalogJson, entries_carry_the_name_and_description_from_the_table) {
  for (std::size_t index = 0; index < catalog().size(); ++index) {
    const cactus::ToolSpec& spec = catalog()[index];

    auto type = cactus::json_string(entries.at(index), "type");
    ASSERT_TRUE(type.has_value()) << spec.name;
    EXPECT_EQ(*type, "function");

    sj::dom::element function = function_of(index);
    auto name = cactus::json_string(function, "name");
    ASSERT_TRUE(name.has_value()) << spec.name;
    EXPECT_EQ(*name, spec.name);

    auto description = cactus::json_string(function, "description");
    ASSERT_TRUE(description.has_value()) << spec.name;
    EXPECT_EQ(*description, spec.description);
  }
}

TEST_F(CatalogJson, describes_every_parameter_of_every_tool) {
  for (std::size_t index = 0; index < catalog().size(); ++index) {
    const cactus::ToolSpec& spec = catalog()[index];
    auto properties = function_of(index).at_pointer("/parameters/properties");
    ASSERT_FALSE(properties.error()) << spec.name;

    for (const cactus::ToolParam& param : spec.params) {
      auto property = properties.at_key(param.name);
      ASSERT_FALSE(property.error()) << spec.name << "." << param.name;
      auto described = cactus::json_string(property.value(), "description");
      ASSERT_TRUE(described.has_value()) << spec.name << "." << param.name;
      EXPECT_EQ(*described, param.description);
    }
  }
}

TEST_F(CatalogJson, marks_exactly_the_required_parameters) {
  for (std::size_t index = 0; index < catalog().size(); ++index) {
    const cactus::ToolSpec& spec = catalog()[index];
    auto required = function_of(index).at_pointer("/parameters/required");
    ASSERT_FALSE(required.error()) << spec.name;

    sj::dom::array names;
    ASSERT_FALSE(required.get(names)) << spec.name;

    std::set<std::string_view> listed;
    for (sj::dom::element item : names) {
      std::string_view text;
      ASSERT_FALSE(item.get(text)) << spec.name;
      listed.insert(text);
    }

    for (const cactus::ToolParam& param : spec.params)
      EXPECT_EQ(param.required, listed.contains(param.name))
          << spec.name << "." << param.name;
  }
}

TEST_F(CatalogJson, describes_lists_as_arrays_of_strings) {
  std::size_t lists_seen = 0;
  for (std::size_t index = 0; index < catalog().size(); ++index) {
    const cactus::ToolSpec& spec = catalog()[index];
    auto properties = function_of(index).at_pointer("/parameters/properties");
    ASSERT_FALSE(properties.error()) << spec.name;

    for (const cactus::ToolParam& param : spec.params) {
      if (param.kind != cactus::ParamKind::PositionalList)
        continue;
      ++lists_seen;
      auto property = properties.at_key(param.name);
      ASSERT_FALSE(property.error());

      auto type = cactus::json_string(property.value(), "type");
      ASSERT_TRUE(type.has_value());
      EXPECT_EQ(*type, "array") << spec.name << "." << param.name;

      auto item_type =
          property.at_pointer("/items/type");
      ASSERT_FALSE(item_type.error()) << spec.name << "." << param.name;
      std::string_view text;
      ASSERT_FALSE(item_type.get(text));
      EXPECT_EQ(text, "string");
    }
  }
  EXPECT_GT(lists_seen, 0u);
}

TEST_F(CatalogJson, booleans_and_integers_keep_their_json_types) {
  auto type_of = [this](std::string_view tool, std::string_view param) {
    for (std::size_t index = 0; index < catalog().size(); ++index) {
      if (catalog()[index].name != tool)
        continue;
      auto properties = function_of(index).at_pointer("/parameters/properties");
      EXPECT_FALSE(properties.error());
      auto property = properties.at_key(param);
      EXPECT_FALSE(property.error());
      return std::string(
          cactus::json_string(property.value(), "type").value_or(""));
    }
    return std::string();
  };

  EXPECT_EQ(type_of("ls", "all"), "boolean");
  EXPECT_EQ(type_of("ls", "path"), "string");
  EXPECT_EQ(type_of("head", "lines"), "integer");
  EXPECT_EQ(type_of("find", "name"), "string");
  EXPECT_EQ(type_of("cat", "paths"), "array");
}

TEST(ToolCatalogJson, is_rendered_once) {
  EXPECT_EQ(&cactus::tool_catalog_json(), &cactus::tool_catalog_json());
}

}  // namespace
