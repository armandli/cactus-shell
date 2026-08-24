#ifndef COMMAND_H
#define COMMAND_H

#include <expected>
#include <string>
#include <string_view>
#include <vector>

#include <needle.h>
#include <tools.h>

namespace cactus {

struct Command {
  std::string program;
  std::vector<std::string> args;  // does not repeat the program
  bool risky = false;             // confirm with the user before running
  bool in_process = false;        // cd: a child process cannot move the shell
};

enum class ExecError : int {
  EmptyCommand = 0,
  BadToolCall,
  MissingArgument,
  UnknownTool,
  ForkFailed,
  StartFailed,
};

std::string_view describe(ExecError error);

// Looks the call up in the tool catalog and fills in argv from its parameters.
// A name that is not in the catalog fails as UnknownTool, so the model can only
// ever run one of the programs the catalog names. Each JSON value becomes
// exactly one argv entry: nothing is split, quoted, or handed to /bin/sh.
std::expected<Command, ExecError> command_from_tool_call(
    const ToolCall& call,
    const ToolCatalog& catalog);

// Renders the command back into a single line for echoing to the user.
std::string render(const Command& command);

// fork + execvp + waitpid. Returns the child's exit status; a child killed by
// a signal reports 128 + signal, matching shell convention.
std::expected<int, ExecError> execute(const Command& command);

}  // namespace cactus

#endif
