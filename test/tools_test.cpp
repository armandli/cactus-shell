#include <set>
#include <span>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include <json_util.h>
#include <tools.h>

namespace {

namespace sj = simdjson;

// Two tools between them cover every ParamKind, both risky marks, the
// in-process exception, and an entry with no parameters at all.
constexpr std::string_view kConfig = R"([
  {
    "name": "ls",
    "program": "ls",
    "description": "List the files at a path",
    "params": [
      { "name": "all", "kind": "flag", "flag": "-a",
        "description": "Include dotfiles" },
      { "name": "lines", "kind": "number", "flag": "-n",
        "description": "How many lines" },
      { "name": "since", "kind": "option", "flag": "--since",
        "description": "Only entries newer than this" },
      { "name": "path", "kind": "positional",
        "description": "Directory to list" },
      { "name": "extras", "kind": "positional_list", "required": true,
        "description": "Anything else to list" }
    ]
  },
  {
    "name": "rm",
    "program": "rm",
    "description": "Delete files permanently",
    "risky": true,
    "params": [
      { "name": "paths", "kind": "positional_list", "required": true,
        "description": "Files to delete" }
    ]
  },
  {
    "name": "pwd",
    "program": "pwd",
    "description": "Print the current directory"
  }
])";

cactus::ToolCatalog parsed(std::string_view json) {
  auto catalog = cactus::ToolCatalog::parse(json);
  if (not catalog.has_value()) {
    ADD_FAILURE() << cactus::describe(catalog.error());
    return cactus::ToolCatalog();
  }
  return std::move(*catalog);
}

cactus::ToolConfigError rejected(std::string_view json) {
  auto catalog = cactus::ToolCatalog::parse(json);
  if (catalog.has_value()) {
    ADD_FAILURE() << "the config was accepted";
    return cactus::ToolConfigError{};
  }
  return catalog.error();
}

// Parses the rendered schema once and hands back the array of tool entries, in
// the same order as the config they came from.
struct CatalogJson : public ::testing::Test {
protected:
  void SetUp() override {
    catalog = parsed(kConfig);
    auto document = cactus::JsonDoc::parse(catalog.schema_json());
    ASSERT_TRUE(document.has_value()) << cactus::describe(document.error());
    doc = std::move(*document);
    ASSERT_FALSE(doc.root().get(entries));
  }

  std::span<const cactus::ToolSpec> specs() const { return catalog.specs(); }

  // The "function" object of the entry at `index`, as an element.
  sj::dom::element function_of(std::size_t index) {
    sj::dom::element entry = entries.at(index);
    auto found = entry.at_key("function");
    EXPECT_FALSE(found.error());
    return found.value();
  }

  cactus::ToolCatalog catalog;
  cactus::JsonDoc doc;
  sj::dom::array entries;
};

TEST(ToolCatalogParse, reads_every_entry_in_order) {
  cactus::ToolCatalog catalog = parsed(kConfig);
  ASSERT_EQ(catalog.specs().size(), 3u);
  EXPECT_EQ(catalog.specs()[0].name, "ls");
  EXPECT_EQ(catalog.specs()[1].name, "rm");
  EXPECT_EQ(catalog.specs()[2].name, "pwd");
  EXPECT_FALSE(catalog.empty());
}

TEST(ToolCatalogParse, reads_the_program_and_the_description) {
  cactus::ToolCatalog catalog = parsed(kConfig);
  const cactus::ToolSpec* spec = catalog.find("ls");
  ASSERT_NE(spec, nullptr);
  EXPECT_EQ(spec->program, "ls");
  EXPECT_EQ(spec->description, "List the files at a path");
}

TEST(ToolCatalogParse, reads_every_parameter_kind) {
  cactus::ToolCatalog catalog = parsed(kConfig);
  const cactus::ToolSpec* spec = catalog.find("ls");
  ASSERT_NE(spec, nullptr);
  ASSERT_EQ(spec->params.size(), 5u);
  EXPECT_EQ(spec->params[0].kind, cactus::ParamKind::Flag);
  EXPECT_EQ(spec->params[0].flag, "-a");
  EXPECT_EQ(spec->params[1].kind, cactus::ParamKind::Number);
  EXPECT_EQ(spec->params[2].kind, cactus::ParamKind::Option);
  EXPECT_EQ(spec->params[2].flag, "--since");
  EXPECT_EQ(spec->params[3].kind, cactus::ParamKind::Positional);
  EXPECT_EQ(spec->params[4].kind, cactus::ParamKind::PositionalList);
}

TEST(ToolCatalogParse, defaults_the_optional_fields) {
  cactus::ToolCatalog catalog = parsed(kConfig);
  const cactus::ToolSpec* pwd = catalog.find("pwd");
  ASSERT_NE(pwd, nullptr);
  EXPECT_TRUE(pwd->params.empty());
  EXPECT_FALSE(pwd->risky);
  EXPECT_FALSE(pwd->in_process);
  EXPECT_TRUE(pwd->triggers.empty());

  const cactus::ToolSpec* ls = catalog.find("ls");
  ASSERT_NE(ls, nullptr);
  EXPECT_FALSE(ls->params[3].required);
  EXPECT_TRUE(ls->params[3].flag.empty());
}

TEST(ToolCatalogParse, carries_the_risky_mark) {
  cactus::ToolCatalog catalog = parsed(kConfig);
  EXPECT_TRUE(catalog.find("rm")->risky);
  EXPECT_FALSE(catalog.find("ls")->risky);
}

TEST(ToolCatalogParse, reads_the_triggers_in_order) {
  cactus::ToolCatalog catalog = parsed(R"([
    {"name": "delete_files", "program": "rm", "description": "Delete",
     "triggers": ["\\b(delete|remove)\\b", "\\berase\\b"]}
  ])");
  const cactus::ToolSpec* spec = catalog.find("delete_files");
  ASSERT_NE(spec, nullptr);
  ASSERT_EQ(spec->triggers.size(), 2u);
  EXPECT_EQ(spec->triggers[0], R"(\b(delete|remove)\b)");
  EXPECT_EQ(spec->triggers[1], R"(\berase\b)");
}

TEST(ToolCatalogParse, renders_triggers_beside_the_description) {
  cactus::ToolCatalog catalog = parsed(R"([
    {"name": "delete_files", "program": "rm", "description": "Delete",
     "triggers": ["\\bdelete\\b"]},
    {"name": "pwd", "program": "pwd", "description": "Where"}
  ])");
  auto doc = cactus::JsonDoc::parse(catalog.schema_json());
  ASSERT_TRUE(doc.has_value());

  auto triggers = doc->root().at_pointer("/0/function/triggers");
  ASSERT_FALSE(triggers.error());
  sj::dom::array items;
  ASSERT_FALSE(triggers.get(items));
  ASSERT_EQ(items.size(), 1u);
  EXPECT_EQ(std::string_view(items.at(0).get_string().value()),
            R"(\bdelete\b)");

  // A tool with no triggers leaves the key out rather than sending [].
  EXPECT_TRUE(doc->root().at_pointer("/1/function/triggers").error());
}

TEST(ToolCatalogParse, accepts_an_empty_catalog) {
  cactus::ToolCatalog catalog = parsed("[]");
  EXPECT_TRUE(catalog.empty());
  EXPECT_EQ(catalog.schema_json(), "[]");
}

TEST(ToolCatalogParse, accepts_cd_as_in_process) {
  cactus::ToolCatalog catalog = parsed(R"([
    {"name": "cd", "program": "cd", "description": "Move", "in_process": true}
  ])");
  ASSERT_NE(catalog.find("cd"), nullptr);
  EXPECT_TRUE(catalog.find("cd")->in_process);
}

TEST(ToolCatalogFind, finds_a_catalog_entry) {
  cactus::ToolCatalog catalog = parsed(kConfig);
  const cactus::ToolSpec* spec = catalog.find("rm");
  ASSERT_NE(spec, nullptr);
  EXPECT_EQ(spec->program, "rm");
}

// Fail-closed: anything the config does not name is unreachable, which is what
// retires the old denylist and its sudo look-through.
TEST(ToolCatalogFind, rejects_a_name_outside_the_catalog) {
  cactus::ToolCatalog catalog = parsed(kConfig);
  EXPECT_EQ(catalog.find("tar"), nullptr);
  EXPECT_EQ(catalog.find("sudo"), nullptr);
  EXPECT_EQ(catalog.find("run_command"), nullptr);
  EXPECT_EQ(catalog.find(""), nullptr);
}

TEST(ToolConfigErrorTest, rejects_text_that_is_not_json) {
  EXPECT_EQ(rejected("not json at all").code,
            cactus::ToolConfigCode::MalformedJson);
}

TEST(ToolConfigErrorTest, rejects_a_root_that_is_not_an_array) {
  EXPECT_EQ(rejected(R"({"name": "ls"})").code,
            cactus::ToolConfigCode::NotAnArray);
}

TEST(ToolConfigErrorTest, rejects_an_entry_that_is_not_an_object) {
  cactus::ToolConfigError error = rejected(R"(["ls"])");
  EXPECT_EQ(error.code, cactus::ToolConfigCode::WrongType);
  EXPECT_EQ(error.where, "entry 0");
}

TEST(ToolConfigErrorTest, rejects_an_entry_with_no_name) {
  cactus::ToolConfigError error =
      rejected(R"([{"program": "ls", "description": "List"}])");
  EXPECT_EQ(error.code, cactus::ToolConfigCode::MissingField);
  EXPECT_EQ(error.where, "entry 0.name");
}

TEST(ToolConfigErrorTest, names_the_offending_entry_by_its_index) {
  cactus::ToolConfigError error = rejected(R"([
    {"name": "ls", "program": "ls", "description": "List"},
    {"name": "pwd", "program": "pwd", "description": "Print"},
    {"program": "rm", "description": "Delete"}
  ])");
  EXPECT_EQ(error.where, "entry 2.name");
}

TEST(ToolConfigErrorTest, rejects_an_entry_with_no_program) {
  cactus::ToolConfigError error =
      rejected(R"([{"name": "ls", "description": "List"}])");
  EXPECT_EQ(error.code, cactus::ToolConfigCode::MissingField);
  EXPECT_EQ(error.where, "ls.program");
}

// The whole point of the catalog is that the model reads these descriptions
// instead of recalling flag spellings, so an undescribed tool is a bug.
TEST(ToolConfigErrorTest, rejects_an_entry_with_no_description) {
  cactus::ToolConfigError error =
      rejected(R"([{"name": "ls", "program": "ls"}])");
  EXPECT_EQ(error.code, cactus::ToolConfigCode::MissingField);
  EXPECT_EQ(error.where, "ls.description");
}

TEST(ToolConfigErrorTest, rejects_an_empty_description) {
  cactus::ToolConfigError error =
      rejected(R"([{"name": "ls", "program": "ls", "description": ""}])");
  EXPECT_EQ(error.code, cactus::ToolConfigCode::MissingField);
  EXPECT_EQ(error.where, "ls.description");
}

TEST(ToolConfigErrorTest, rejects_a_field_of_the_wrong_type) {
  cactus::ToolConfigError error =
      rejected(R"([{"name": "ls", "program": 7, "description": "List"}])");
  EXPECT_EQ(error.code, cactus::ToolConfigCode::WrongType);
  EXPECT_EQ(error.where, "ls.program");
}

TEST(ToolConfigErrorTest, rejects_a_risky_mark_that_is_not_a_boolean) {
  cactus::ToolConfigError error = rejected(
      R"([{"name": "rm", "program": "rm", "description": "D",)"
      R"( "risky": "yes"}])");
  EXPECT_EQ(error.code, cactus::ToolConfigCode::WrongType);
  EXPECT_EQ(error.where, "rm.risky");
}

TEST(ToolConfigErrorTest, rejects_triggers_that_are_not_an_array) {
  cactus::ToolConfigError error = rejected(
      R"([{"name": "ls", "program": "ls", "description": "List",)"
      R"( "triggers": "\\blist\\b"}])");
  EXPECT_EQ(error.code, cactus::ToolConfigCode::WrongType);
  EXPECT_EQ(error.where, "ls.triggers");
}

TEST(ToolConfigErrorTest, rejects_a_trigger_that_is_not_a_string) {
  cactus::ToolConfigError error = rejected(
      R"([{"name": "ls", "program": "ls", "description": "List",)"
      R"( "triggers": ["\\blist\\b", 7]}])");
  EXPECT_EQ(error.code, cactus::ToolConfigCode::WrongType);
  EXPECT_EQ(error.where, "ls.triggers");
}

TEST(ToolConfigErrorTest, rejects_an_empty_trigger) {
  cactus::ToolConfigError error = rejected(
      R"([{"name": "ls", "program": "ls", "description": "List",)"
      R"( "triggers": [""]}])");
  EXPECT_EQ(error.code, cactus::ToolConfigCode::MissingField);
  EXPECT_EQ(error.where, "ls.triggers");
}

TEST(ToolConfigErrorTest, rejects_a_repeated_tool_name) {
  cactus::ToolConfigError error = rejected(R"([
    {"name": "ls", "program": "ls", "description": "List"},
    {"name": "ls", "program": "dir", "description": "List again"}
  ])");
  EXPECT_EQ(error.code, cactus::ToolConfigCode::DuplicateName);
  EXPECT_EQ(error.where, "ls");
}

TEST(ToolConfigErrorTest, rejects_a_repeated_parameter_name) {
  cactus::ToolConfigError error = rejected(R"([
    {"name": "ls", "program": "ls", "description": "List", "params": [
      {"name": "path", "kind": "positional", "description": "First"},
      {"name": "path", "kind": "positional", "description": "Again"}
    ]}
  ])");
  EXPECT_EQ(error.code, cactus::ToolConfigCode::DuplicateName);
  EXPECT_EQ(error.where, "ls.path");
}

TEST(ToolConfigErrorTest, rejects_an_unknown_parameter_kind) {
  cactus::ToolConfigError error = rejected(R"([
    {"name": "ls", "program": "ls", "description": "List", "params": [
      {"name": "path", "kind": "pathname", "description": "Where"}
    ]}
  ])");
  EXPECT_EQ(error.code, cactus::ToolConfigCode::UnknownParamKind);
  EXPECT_EQ(error.where, "ls.path.kind");
}

TEST(ToolConfigErrorTest, rejects_an_undescribed_parameter) {
  cactus::ToolConfigError error = rejected(R"([
    {"name": "ls", "program": "ls", "description": "List", "params": [
      {"name": "path", "kind": "positional"}
    ]}
  ])");
  EXPECT_EQ(error.code, cactus::ToolConfigCode::MissingField);
  EXPECT_EQ(error.where, "ls.path.description");
}

// The argv builder emits param.flag for these kinds and nothing but the value
// for the others, so a mismatch would silently produce a broken argv.
TEST(ToolConfigErrorTest, rejects_a_flagged_kind_with_no_flag) {
  for (std::string_view kind : {"flag", "option", "number"}) {
    cactus::ToolConfigError error = rejected(
        std::string(R"([{"name": "ls", "program": "ls", "description": "L",)"
                    R"( "params": [{"name": "all", "kind": ")") +
        std::string(kind) + R"(", "description": "All"}]}])");
    EXPECT_EQ(error.code, cactus::ToolConfigCode::FlagMismatch) << kind;
    EXPECT_EQ(error.where, "ls.all.flag") << kind;
  }
}

TEST(ToolConfigErrorTest, rejects_a_positional_kind_carrying_a_flag) {
  for (std::string_view kind : {"positional", "positional_list"}) {
    cactus::ToolConfigError error = rejected(
        std::string(R"([{"name": "ls", "program": "ls", "description": "L",)"
                    R"( "params": [{"name": "path", "kind": ")") +
        std::string(kind) + R"(", "flag": "-p", "description": "Where"}]}])");
    EXPECT_EQ(error.code, cactus::ToolConfigCode::FlagMismatch) << kind;
    EXPECT_EQ(error.where, "ls.path.flag") << kind;
  }
}

// Shell::change_directory is the only in-process implementation there is, so a
// config claiming anything else runs in process would silently cd instead.
TEST(ToolConfigErrorTest, rejects_in_process_on_anything_but_cd) {
  cactus::ToolConfigError error = rejected(
      R"([{"name": "go", "program": "rm", "description": "D",)"
      R"( "in_process": true}])");
  EXPECT_EQ(error.code, cactus::ToolConfigCode::InProcessNotAllowed);
  EXPECT_EQ(error.where, "go");
}

TEST(ToolConfigErrorTest, rejects_params_that_are_not_an_array) {
  cactus::ToolConfigError error = rejected(
      R"([{"name": "ls", "program": "ls", "description": "L", "params": {}}])");
  EXPECT_EQ(error.code, cactus::ToolConfigCode::WrongType);
  EXPECT_EQ(error.where, "ls.params");
}

TEST(ToolConfigErrorTest, every_code_has_a_description) {
  for (cactus::ToolConfigCode code :
       {cactus::ToolConfigCode::NoConfigFound,
        cactus::ToolConfigCode::FileUnreadable,
        cactus::ToolConfigCode::MalformedJson,
        cactus::ToolConfigCode::NotAnArray,
        cactus::ToolConfigCode::MissingField,
        cactus::ToolConfigCode::WrongType,
        cactus::ToolConfigCode::UnknownParamKind,
        cactus::ToolConfigCode::DuplicateName,
        cactus::ToolConfigCode::FlagMismatch,
        cactus::ToolConfigCode::InProcessNotAllowed})
    EXPECT_FALSE(cactus::describe(cactus::ToolConfigError{code, {}}).empty());
}

TEST(ToolConfigErrorTest, the_description_names_the_offending_place) {
  std::string text = cactus::describe(
      cactus::ToolConfigError{cactus::ToolConfigCode::MissingField, "ls.path"});
  EXPECT_NE(text.find("ls.path"), std::string::npos);
}

TEST(ToolCatalogLoad, reports_a_file_that_is_not_there) {
  auto catalog = cactus::ToolCatalog::load("/no/such/cactus-tools.json");
  ASSERT_FALSE(catalog.has_value());
  EXPECT_EQ(catalog.error().code, cactus::ToolConfigCode::FileUnreadable);
  EXPECT_NE(catalog.error().where.find("cactus-tools.json"),
            std::string::npos);
}

TEST_F(CatalogJson, is_an_array_with_one_entry_per_spec) {
  EXPECT_TRUE(doc.root().is_array());
  EXPECT_EQ(entries.size(), specs().size());
}

TEST_F(CatalogJson, entries_carry_the_name_and_description_from_the_config) {
  for (std::size_t index = 0; index < specs().size(); ++index) {
    const cactus::ToolSpec& spec = specs()[index];

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
  for (std::size_t index = 0; index < specs().size(); ++index) {
    const cactus::ToolSpec& spec = specs()[index];
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
  for (std::size_t index = 0; index < specs().size(); ++index) {
    const cactus::ToolSpec& spec = specs()[index];
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
  for (std::size_t index = 0; index < specs().size(); ++index) {
    const cactus::ToolSpec& spec = specs()[index];
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

      auto item_type = property.at_pointer("/items/type");
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
    for (std::size_t index = 0; index < specs().size(); ++index) {
      if (specs()[index].name != tool)
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
  EXPECT_EQ(type_of("ls", "lines"), "integer");
  EXPECT_EQ(type_of("ls", "since"), "string");
  EXPECT_EQ(type_of("ls", "path"), "string");
  EXPECT_EQ(type_of("rm", "paths"), "array");
}

}  // namespace
