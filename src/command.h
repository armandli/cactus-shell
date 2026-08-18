#ifndef COMMAND_H
#define COMMAND_H

#include <expected>
#include <string>
#include <string_view>
#include <vector>

#include <needle.h>

namespace cactus {

struct Command {
  std::string program;
  std::vector<std::string> args;  // does not repeat the program
};

enum class ExecError : int {
  EmptyCommand = 0,
  BadToolCall,
  UnknownTool,
  ForkFailed,
  StartFailed,
};

std::string_view describe(ExecError error);

// Reads {"command": "..."} out of ToolCall::arguments and tokenizes it. The
// command line never reaches /bin/sh: it is model-generated, so routing it
// through a shell would turn every mistranslation into a metacharacter hazard.
std::expected<Command, ExecError> command_from_tool_call(const ToolCall& call);

// True when the program is destructive enough to warrant a confirmation.
bool is_risky(const Command& command);

// Renders the command back into a single line for echoing to the user.
std::string render(const Command& command);

// fork + execvp + waitpid. Returns the child's exit status; a child killed by
// a signal reports 128 + signal, matching shell convention.
std::expected<int, ExecError> execute(const Command& command);

}  // namespace cactus

#endif
