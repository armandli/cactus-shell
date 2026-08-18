#include <command.h>

#include <cassert>
#include <cerrno>

#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <array>

#include <json_util.h>
#include <tokenize.h>

namespace cactus {

namespace {

constexpr std::string_view kRunCommand = "run_command";

constexpr std::array<std::string_view, 15> kRiskyPrograms = {
    "rm", "rmdir", "dd", "mkfs", "shutdown", "reboot", "halt", "chmod",
    "chown", "mv", "kill", "killall", "truncate", "shred", "fdisk",
};

std::string_view basename(std::string_view path) {
  std::size_t slash = path.rfind('/');
  if (slash == std::string_view::npos)
    return path;
  return path.substr(slash + 1);
}

bool listed(std::string_view program) {
  return std::ranges::find(kRiskyPrograms, program) != kRiskyPrograms.end();
}

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
  if (call.name != kRunCommand)
    return std::unexpected(ExecError::UnknownTool);

  auto doc = JsonDoc::parse(call.arguments);
  if (not doc.has_value())
    return std::unexpected(ExecError::BadToolCall);

  auto line = json_string(doc->root(), "command");
  if (not line.has_value())
    return std::unexpected(ExecError::BadToolCall);

  auto words = tokenize(*line);
  if (not words.has_value())
    return std::unexpected(ExecError::BadToolCall);
  if (words->empty())
    return std::unexpected(ExecError::EmptyCommand);

  Command command;
  command.program = words->front();
  command.args.assign(words->begin() + 1, words->end());
  return command;
}

bool is_risky(const Command& command) {
  std::string_view program = basename(command.program);
  // sudo hides the real program behind its own options, and those options can
  // take values, so anything that looks like a risky program anywhere in the
  // argument list counts.
  if (program == "sudo") {
    for (const std::string& arg : command.args)
      if (listed(basename(arg)))
        return true;
    return true;
  }
  return listed(program);
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
  if (::pipe2(report, O_CLOEXEC) != 0)
    return std::unexpected(ExecError::ForkFailed);

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
