#ifndef TOOLS_H
#define TOOLS_H

#include <span>
#include <string>
#include <string_view>

namespace cactus {

enum class ParamKind : int {
  Flag = 0,        // boolean -> emits `flag` when true, nothing when false
  Option,          // string  -> emits `flag` then the value
  Number,          // integer -> emits `flag` then the digits
  Positional,      // string  -> emits the value
  PositionalList,  // array of strings -> emits each value
};

struct ToolParam {
  std::string_view name;  // the JSON key the model fills in
  std::string_view description;
  ParamKind kind;
  std::string_view flag;  // argv token for Flag/Option/Number, else empty
  bool required;
};

struct ToolSpec {
  std::string_view name;     // what the model calls, e.g. "ls"
  std::string_view program;  // what gets exec'd
  std::string_view description;
  std::span<const ToolParam> params;
  bool risky;       // needs confirmation before running
  bool in_process;  // cd only: a child process cannot move us
};

// The complete set of programs this shell can run. The model picks from this
// table and can never name anything outside it, so the catalog is the whole
// attack surface. Parameters become argv in declaration order, so each spec
// lists its parameters in the order that program expects them.
std::span<const ToolSpec> tool_catalog();

const ToolSpec* find_tool(std::string_view name);

// The catalog as an OpenAI-style tool array, rendered from the same table that
// builds the argv so the two cannot drift apart.
const std::string& tool_catalog_json();

}  // namespace cactus

#endif
