#include <command.h>

#include <cassert>
#include <cerrno>
#include <cstdint>

#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

#include <string>

#include <json_util.h>
#include <tools.h>

namespace cactus {

namespace sj = simdjson;

namespace {

bool needs_quoting(std::string_view word) {
  if (word.empty())
    return true;
  return word.find_first_of(" \t\n'\"\\") != std::string_view::npos;
}

}  // namespace

std::string_view describe(ExecError error) {
  switch (error) {
    case ExecError::EmptyCommand:
      return "the model produced an empty command";

    break; case ExecError::BadToolCall:
      return "the model's tool call could not be read as a command";

    break; case ExecError::MissingArgument:
      return "the model's tool call left out a required argument";

    break; case ExecError::UnknownTool:
      return "the model called a tool this shell does not provide";

    break; case ExecError::ForkFailed:
      return "could not create a process";

    break; case ExecError::StartFailed:
      return "could not start the program";

    break; default:
      assert(false); // should never get here
      return "unknown execution error";
  }
}

std::expected<Command, ExecError> command_from_tool_call(const ToolCall& call)
{
  const ToolSpec* spec = find_tool(call.name);
  if (spec == nullptr)
    return std::unexpected(ExecError::UnknownTool);

  auto doc = JsonDoc::parse(call.arguments);
  if (not doc.has_value())
    return std::unexpected(ExecError::BadToolCall);
  sj::dom::element root = doc->root();

  if (not root.is_object())
    return std::unexpected(ExecError::BadToolCall);

  Command command;
  command.program = spec->program;
  command.risky = spec->risky;
  command.in_process = spec->in_process;

  for (const ToolParam& param : spec->params) {
    auto slot = root.at_key(param.name);
    if (slot.error()) {
      if (param.required)
        return std::unexpected(ExecError::MissingArgument);
      continue;
    }
    sj::dom::element value = slot.value();

    switch (param.kind) {
      case ParamKind::Flag: {
        bool set = false;
        if (value.get(set))
          return std::unexpected(ExecError::BadToolCall);
        if (set)
          command.args.emplace_back(param.flag);
      }

      break; case ParamKind::Option: {
        std::string_view text;
        if (value.get(text))
          return std::unexpected(ExecError::BadToolCall);
        command.args.emplace_back(param.flag);
        command.args.emplace_back(text);
      }

      break; case ParamKind::Number: {
        std::int64_t number = 0;
        if (value.get(number))
          return std::unexpected(ExecError::BadToolCall);
        command.args.emplace_back(param.flag);
        command.args.push_back(std::to_string(number));
      }

      break; case ParamKind::Positional: {
        std::string_view text;
        if (value.get(text))
          return std::unexpected(ExecError::BadToolCall);
        command.args.emplace_back(text);
      }

      break; case ParamKind::PositionalList: {
        sj::dom::array items;
        if (value.get(items))
          return std::unexpected(ExecError::BadToolCall);
        for (sj::dom::element item : items) {
          std::string_view text;
          if (item.get(text))
            return std::unexpected(ExecError::BadToolCall);
          command.args.emplace_back(text);
        }
      }

      break; default:
        assert(false); // should never get here
        return std::unexpected(ExecError::BadToolCall);
    }
  }

  return command;
}

std::string render(const Command& command) {
  std::string line;
  auto append = [&line](const std::string& word) {
    if (not line.empty())
      line.push_back(' ');
    if (not needs_quoting(word)) {
      line.append(word);
      return;
    }
    line.push_back('"');
    for (char c : word) {
      if (c == '"' or c == '\\')
        line.push_back('\\');
      line.push_back(c);
    }
    line.push_back('"');
  };

  append(command.program);
  for (const std::string& arg : command.args)
    append(arg);
  return line;
}

std::expected<int, ExecError> execute(const Command& command) {
  if (command.program.empty())
    return std::unexpected(ExecError::EmptyCommand);

  std::vector<char*> argv;
  argv.reserve(command.args.size() + 2);
  argv.push_back(const_cast<char*>(command.program.data()));
  for (const std::string& arg : command.args)
    argv.push_back(const_cast<char*>(arg.data()));
  argv.push_back(nullptr);

  // The child reports an execvp failure through this pipe; the write end is
  // close-on-exec, so a successful exec closes it and the parent reads nothing.
  int report[2];
  if (::pipe(report) != 0)
    return std::unexpected(ExecError::ForkFailed);
  if (::fcntl(report[0], F_SETFD, FD_CLOEXEC) != 0 or
      ::fcntl(report[1], F_SETFD, FD_CLOEXEC) != 0) {
    ::close(report[0]);
    ::close(report[1]);
    return std::unexpected(ExecError::ForkFailed);
  }

  pid_t child = ::fork();
  if (child < 0) {
    ::close(report[0]);
    ::close(report[1]);
    return std::unexpected(ExecError::ForkFailed);
  }

  if (child == 0) {
    ::close(report[0]);
    ::execvp(argv[0], argv.data());
    int failure = errno;
    ssize_t ignored = ::write(report[1], &failure, sizeof(failure));
    static_cast<void>(ignored);
    ::_exit(127);
  }

  ::close(report[1]);
  int failure = 0;
  ssize_t received = ::read(report[0], &failure, sizeof(failure));
  ::close(report[0]);

  int status = 0;
  while (::waitpid(child, &status, 0) < 0)
    if (errno != EINTR)
      return std::unexpected(ExecError::ForkFailed);

  if (received > 0)
    return std::unexpected(ExecError::StartFailed);
  if (WIFSIGNALED(status))
    return 128 + WTERMSIG(status);
  return WEXITSTATUS(status);
}

}  // namespace cactus
