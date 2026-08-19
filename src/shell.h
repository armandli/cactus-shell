#ifndef SHELL_H
#define SHELL_H

#include <iosfwd>
#include <string>
#include <string_view>

#include <command.h>
#include <needle.h>

namespace cactus {

struct ShellConfig {
  std::string model_path;
  // Needle answers tool-calling prompts far more reliably with no system
  // message at all than with one: a system message pushes it off the
  // distribution it was tuned on and it starts emitting garbled, duplicated
  // tool calls. The run_command tool's own name and description carry
  // enough context on their own.
  std::string system_prompt;
  bool confirm_risky = true;

  static ShellConfig from_args(int argc, char** argv);
};

// Reads natural language, asks the Needle model what command it means, and
// runs that command. Input and output are injectable so that the loop, the
// builtins, and the confirmation prompt can be tested without a model.
struct Shell {
  Shell() = default;

  explicit Shell(ShellConfig config)
    : config_(std::move(config)), client_(config_.system_prompt) {}

  int run();
  int run(std::istream& in, std::ostream& out);

protected:
  enum class Action : int {
    Continue = 0,
    Quit,
  };

  Action handle_line(
      std::string_view line,
      std::istream& in,
      std::ostream& out);

  Action handle_builtin(std::string_view line, std::ostream& out, bool& handled);

  Action handle_request(
      std::string_view line,
      std::istream& in,
      std::ostream& out);

  bool confirm(const Command& command, std::istream& in, std::ostream& out);
  bool ensure_model(std::ostream& out);

  std::string prompt() const { return "cactus$ "; }

  ShellConfig config_;
  NeedleClient client_;
  bool model_ready_ = false;
};

}  // namespace cactus

#endif
