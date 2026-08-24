#include <tools.h>

#include <cassert>
#include <cstdlib>

#include <algorithm>
#include <fstream>
#include <iterator>
#include <system_error>

#include <json_util.h>

namespace cactus {

namespace sj = simdjson;

namespace {

std::string field_of(std::string_view label, std::string_view key) {
  return std::string(label) + "." + std::string(key);
}

std::string_view describe(ToolConfigCode code) {
  switch (code) {
    case ToolConfigCode::NoConfigFound:
      return "no tool configuration file found";

    break; case ToolConfigCode::FileUnreadable:
      return "the tool configuration file could not be read";

    break; case ToolConfigCode::MalformedJson:
      return "the tool configuration is not valid JSON";

    break; case ToolConfigCode::NotAnArray:
      return "the tool configuration must be an array of tools";

    break; case ToolConfigCode::MissingField:
      return "a required field is missing or empty";

    break; case ToolConfigCode::WrongType:
      return "a field has the wrong type";

    break; case ToolConfigCode::UnknownParamKind:
      return "a parameter kind is not one of flag, option, number, "
             "positional, positional_list";

    break; case ToolConfigCode::DuplicateName:
      return "a name is used twice";

    break; case ToolConfigCode::FlagMismatch:
      return "flag, option and number parameters need a flag spelling; "
             "positional ones must not have one";

    break; case ToolConfigCode::InProcessNotAllowed:
      return "only the cd program may run in process";

    break; default:
      assert(false); // should never get here
      return "unknown tool configuration error";
  }
}

std::expected<std::string, ToolConfigError> required_string(
    sj::dom::element parent,
    std::string_view key,
    std::string_view label)
{
  auto slot = parent.at_key(key);
  if (slot.error())
    return std::unexpected(
        ToolConfigError{ToolConfigCode::MissingField, field_of(label, key)});
  std::string_view text;
  if (slot.value().get(text))
    return std::unexpected(
        ToolConfigError{ToolConfigCode::WrongType, field_of(label, key)});
  if (text.empty())
    return std::unexpected(
        ToolConfigError{ToolConfigCode::MissingField, field_of(label, key)});
  return std::string(text);
}

std::expected<std::string, ToolConfigError> optional_string(
    sj::dom::element parent,
    std::string_view key,
    std::string_view label)
{
  auto slot = parent.at_key(key);
  if (slot.error())
    return std::string();
  std::string_view text;
  if (slot.value().get(text))
    return std::unexpected(
        ToolConfigError{ToolConfigCode::WrongType, field_of(label, key)});
  return std::string(text);
}

std::expected<bool, ToolConfigError> optional_bool(
    sj::dom::element parent,
    std::string_view key,
    std::string_view label)
{
  auto slot = parent.at_key(key);
  if (slot.error())
    return false;
  bool set = false;
  if (slot.value().get(set))
    return std::unexpected(
        ToolConfigError{ToolConfigCode::WrongType, field_of(label, key)});
  return set;
}

std::expected<ParamKind, ToolConfigError> parse_kind(
    std::string_view text,
    std::string_view label)
{
  if (text == "flag")
    return ParamKind::Flag;
  if (text == "option")
    return ParamKind::Option;
  if (text == "number")
    return ParamKind::Number;
  if (text == "positional")
    return ParamKind::Positional;
  if (text == "positional_list")
    return ParamKind::PositionalList;
  return std::unexpected(ToolConfigError{ToolConfigCode::UnknownParamKind,
                                         field_of(label, "kind")});
}

bool carries_a_flag(ParamKind kind) {
  return kind == ParamKind::Flag or kind == ParamKind::Option or
         kind == ParamKind::Number;
}

std::expected<ToolParam, ToolConfigError> read_param(
    sj::dom::element entry,
    std::string_view tool)
{
  if (not entry.is_object())
    return std::unexpected(
        ToolConfigError{ToolConfigCode::WrongType, field_of(tool, "params")});

  auto name = required_string(entry, "name", field_of(tool, "params"));
  if (not name.has_value())
    return std::unexpected(name.error());

  std::string label = field_of(tool, *name);

  auto description = required_string(entry, "description", label);
  if (not description.has_value())
    return std::unexpected(description.error());

  auto kind_text = required_string(entry, "kind", label);
  if (not kind_text.has_value())
    return std::unexpected(kind_text.error());
  auto kind = parse_kind(*kind_text, label);
  if (not kind.has_value())
    return std::unexpected(kind.error());

  auto flag = optional_string(entry, "flag", label);
  if (not flag.has_value())
    return std::unexpected(flag.error());

  auto required = optional_bool(entry, "required", label);
  if (not required.has_value())
    return std::unexpected(required.error());

  // The argv builder emits param.flag for the flagged kinds and nothing but
  // the value for the others, so a mismatch here would build a broken argv.
  if (carries_a_flag(*kind) == flag->empty())
    return std::unexpected(ToolConfigError{ToolConfigCode::FlagMismatch,
                                           field_of(label, "flag")});

  ToolParam param;
  param.name = std::move(*name);
  param.description = std::move(*description);
  param.kind = *kind;
  param.flag = std::move(*flag);
  param.required = *required;
  return param;
}

std::expected<ToolSpec, ToolConfigError> read_spec(
    sj::dom::element entry,
    std::size_t index)
{
  std::string label = "entry " + std::to_string(index);
  if (not entry.is_object())
    return std::unexpected(ToolConfigError{ToolConfigCode::WrongType, label});

  auto name = required_string(entry, "name", label);
  if (not name.has_value())
    return std::unexpected(name.error());
  label = *name;

  auto program = required_string(entry, "program", label);
  if (not program.has_value())
    return std::unexpected(program.error());

  auto description = required_string(entry, "description", label);
  if (not description.has_value())
    return std::unexpected(description.error());

  auto risky = optional_bool(entry, "risky", label);
  if (not risky.has_value())
    return std::unexpected(risky.error());

  auto in_process = optional_bool(entry, "in_process", label);
  if (not in_process.has_value())
    return std::unexpected(in_process.error());

  // Shell::change_directory is the only in-process implementation there is.
  if (*in_process and *program != "cd")
    return std::unexpected(
        ToolConfigError{ToolConfigCode::InProcessNotAllowed, label});

  ToolSpec spec;
  spec.name = std::move(*name);
  spec.program = std::move(*program);
  spec.description = std::move(*description);
  spec.risky = *risky;
  spec.in_process = *in_process;

  auto slot = entry.at_key("params");
  if (slot.error())
    return spec;

  sj::dom::array items;
  if (slot.value().get(items))
    return std::unexpected(
        ToolConfigError{ToolConfigCode::WrongType, field_of(label, "params")});

  for (sj::dom::element item : items) {
    auto param = read_param(item, label);
    if (not param.has_value())
      return std::unexpected(param.error());
    bool repeated = std::ranges::any_of(
        spec.params,
        [&param](const ToolParam& seen) { return seen.name == param->name; });
    if (repeated)
      return std::unexpected(ToolConfigError{ToolConfigCode::DuplicateName,
                                             field_of(label, param->name)});
    spec.params.push_back(std::move(*param));
  }

  return spec;
}

std::string_view json_type(ParamKind kind) {
  switch (kind) {
    case ParamKind::Flag:
      return "boolean";

    break; case ParamKind::Option:
      return "string";

    break; case ParamKind::Number:
      return "integer";

    break; case ParamKind::Positional:
      return "string";

    break; case ParamKind::PositionalList:
      return "array";

    break; default:
      assert(false); // should never get here
      return "string";
  }
}

std::string render_catalog(std::span<const ToolSpec> specs) {
  JsonBuilder builder;
  builder.begin_array();

  for (const ToolSpec& spec : specs) {
    builder.begin_object()
        .field("type", std::string_view{"function"})
        .key("function")
        .begin_object()
        .field("name", spec.name)
        .field("description", spec.description)
        .key("parameters")
        .begin_object()
        .field("type", std::string_view{"object"})
        .key("properties")
        .begin_object();

    for (const ToolParam& param : spec.params) {
      builder.key(param.name)
          .begin_object()
          .field("type", json_type(param.kind))
          .field("description", param.description);
      if (param.kind == ParamKind::PositionalList) {
        builder.key("items")
            .begin_object()
            .field("type", std::string_view{"string"})
            .end_object();
      }
      builder.end_object();
    }

    builder.end_object().key("required").begin_array();
    for (const ToolParam& param : spec.params)
      if (param.required)
        builder.value(param.name);
    builder.end_array();

    builder.end_object().end_object().end_object();
  }

  builder.end_array();
  return builder.str().value_or(std::string{"[]"});
}

std::vector<std::filesystem::path> config_candidates() {
  std::vector<std::filesystem::path> candidates;
  candidates.emplace_back("cactus-tools.json");
  const char* home = std::getenv("HOME");
  if (home != nullptr)
    candidates.push_back(std::filesystem::path(home) / ".config" /
                         "cactus-shell" / "tools.json");
  candidates.emplace_back(CACTUS_DEFAULT_TOOL_CONFIG);
  return candidates;
}

}  // namespace

std::string describe(const ToolConfigError& error) {
  std::string text(describe(error.code));
  if (not error.where.empty())
    text += " (" + error.where + ")";
  return text;
}

std::expected<ToolCatalog, ToolConfigError> ToolCatalog::parse(
    std::string_view json)
{
  auto doc = JsonDoc::parse(json);
  if (not doc.has_value())
    return std::unexpected(ToolConfigError{ToolConfigCode::MalformedJson, {}});

  sj::dom::array entries;
  if (doc->root().get(entries))
    return std::unexpected(ToolConfigError{ToolConfigCode::NotAnArray, {}});

  ToolCatalog catalog;
  std::size_t index = 0;
  for (sj::dom::element entry : entries) {
    auto spec = read_spec(entry, index);
    if (not spec.has_value())
      return std::unexpected(spec.error());
    if (catalog.find(spec->name) != nullptr)
      return std::unexpected(
          ToolConfigError{ToolConfigCode::DuplicateName, spec->name});
    catalog.mSpecs.push_back(std::move(*spec));
    ++index;
  }

  catalog.mSchemaJson = render_catalog(catalog.mSpecs);
  return catalog;
}

std::expected<ToolCatalog, ToolConfigError> ToolCatalog::load(
    const std::filesystem::path& path)
{
  std::ifstream file(path, std::ios::binary);
  if (not file)
    return std::unexpected(
        ToolConfigError{ToolConfigCode::FileUnreadable, path.string()});

  std::string text((std::istreambuf_iterator<char>(file)),
                   std::istreambuf_iterator<char>());
  if (file.bad())
    return std::unexpected(
        ToolConfigError{ToolConfigCode::FileUnreadable, path.string()});

  auto catalog = parse(text);
  if (catalog.has_value())
    return catalog;

  // The message is the user's only handle on a file they have to go edit, so
  // name the file alongside whatever the parse found.
  ToolConfigError error = catalog.error();
  error.where = error.where.empty() ? path.string()
                                    : path.string() + ": " + error.where;
  return std::unexpected(error);
}

std::expected<ToolCatalog, ToolConfigError> ToolCatalog::discover() {
  std::vector<std::filesystem::path> candidates = config_candidates();

  std::error_code ignored;
  for (const std::filesystem::path& candidate : candidates)
    if (std::filesystem::is_regular_file(candidate, ignored))
      return load(candidate);

  std::string looked = "looked in";
  for (const std::filesystem::path& candidate : candidates)
    looked += " " + candidate.string();
  return std::unexpected(
      ToolConfigError{ToolConfigCode::NoConfigFound, std::move(looked)});
}

const ToolSpec* ToolCatalog::find(std::string_view name) const {
  auto found = std::ranges::find(mSpecs, name, &ToolSpec::name);
  return found == mSpecs.end() ? nullptr : &*found;
}

}  // namespace cactus
