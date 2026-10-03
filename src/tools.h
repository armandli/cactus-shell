#ifndef TOOLS_H
#define TOOLS_H

#include <expected>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace cactus {

enum class ParamKind : int {
  Flag = 0,        // boolean -> emits `flag` when true, nothing when false
  Option,          // string  -> emits `flag` then the value
  Number,          // integer -> emits `flag` then the digits
  Positional,      // string  -> emits the value
  PositionalList,  // array of strings -> emits each value
};

struct ToolParam {
  std::string name;  // the JSON key the model fills in
  std::string description;
  ParamKind kind = ParamKind::Positional;
  std::string flag;  // argv token for Flag/Option/Number, else empty
  bool required = false;
};

struct ToolSpec {
  std::string name;     // what the model calls, e.g. "list_files"
  std::string program;  // what gets exec'd
  std::string description;
  // Regexes over the request. Needle 3 narrows its choice to the tools whose
  // triggers match, which is what keeps a tool reachable in a catalog too
  // large for the model to see whole.
  std::vector<std::string> triggers;
  std::vector<ToolParam> params;
  bool risky = false;       // needs confirmation before running
  bool in_process = false;  // cd only: a child process cannot move us
};

enum class ToolConfigCode : int {
  NoConfigFound = 0,
  FileUnreadable,
  MalformedJson,
  NotAnArray,
  MissingField,
  WrongType,
  UnknownParamKind,
  DuplicateName,
  FlagMismatch,
  InProcessNotAllowed,
};

// A code alone cannot say *which* entry is wrong, and this is a file a human
// edits, so the error carries the location: "ls.path", "entry 7".
struct ToolConfigError {
  ToolConfigCode code = ToolConfigCode::MalformedJson;
  std::string where;
};

std::string describe(const ToolConfigError& error);

// The complete set of programs this shell can run, read from a JSON config file
// at startup. The model picks an entry from the loaded catalog and can never
// name anything outside it. Parameters become argv in declaration order, so
// each entry lists its parameters in the order that program expects them.
//
// The config file is as trusted as the binary: whatever it names, the shell
// will run.
struct ToolCatalog {
  ToolCatalog() = default;

  // The testable seam — pure text in, catalog out, no filesystem.
  static std::expected<ToolCatalog, ToolConfigError> parse(
      std::string_view json);

  static std::expected<ToolCatalog, ToolConfigError> load(
      const std::filesystem::path& path);

  // Reads ./cactus-tools.json, then ~/.config/cactus-shell/tools.json, then
  // the path baked in as CACTUS_DEFAULT_TOOL_CONFIG. The first that exists
  // wins; nothing found is NoConfigFound.
  static std::expected<ToolCatalog, ToolConfigError> discover();

  const ToolSpec* find(std::string_view name) const;

  std::span<const ToolSpec> specs() const { return mSpecs; }

  // The catalog as an OpenAI-style tool array, rendered from the same specs
  // that build the argv so the two cannot drift apart.
  const std::string& schema_json() const { return mSchemaJson; }

  bool empty() const { return mSpecs.empty(); }

protected:
  std::vector<ToolSpec> mSpecs;
  std::string mSchemaJson;
};

}  // namespace cactus

#endif
