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
  std::string system_prompt =
      "You are a shell. Translate the user's request into a single command "
      "line for their machine and call run_command with it. Answer in words "
      "only when no command could satisfy the request.";
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
