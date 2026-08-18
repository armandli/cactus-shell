#include <shell.h>

#include <cstdlib>

#include <unistd.h>

#include <iostream>
#include <string>

#include <command.h>
#include <needle.h>
#include <tokenize.h>

namespace cactus {

namespace {

// The one tool the model may call. Keeping it to a single command string means
// every reply lands in the same place: tokenize, then execvp.
constexpr std::string_view kRunCommandTools = R"([{
  "type": "function",
  "function": {
    "name": "run_command",
    "description": "Run a command line on the user's machine",
    "parameters": {
      "type": "object",
      "properties": {
        "command": {
          "type": "string",
          "description": "The complete command line, e.g. 'ls -la /tmp'"
        }
      },
      "required": ["command"]
    }
  }
}])";

std::string_view trim(std::string_view text) {
  std::size_t begin = text.find_first_not_of(" \t\r\n");
  if (begin == std::string_view::npos)
    return {};
  std::size_t end = text.find_last_not_of(" \t\r\n");
  return text.substr(begin, end - begin + 1);
}

}  // namespace

ShellConfig ShellConfig::from_args(int argc, char** argv) {
  ShellConfig config;
  if (argc > 1 and argv[1] != nullptr) {
    config.model_path = argv[1];
    return config;
  }
  const char* from_env = std::getenv("CACTUS_NEEDLE_MODEL");
  if (from_env != nullptr)
    config.model_path = from_env;
  return config;
}

int Shell::run() {
  return run(std::cin, std::cout);
}

int Shell::run(std::istream& in, std::ostream& out) {
  out << "cactus shell 0.1.0 - say what you want, or 'exit' to leave\n";

  std::string line;
  for (;;) {
    out << prompt() << std::flush;
    if (not std::getline(in, line))
      break;
    if (handle_line(line, in, out) == Action::Quit)
      break;
  }

  out << std::endl;
  return 0;
}

Shell::Action Shell::handle_line(
    std::string_view line,
    std::istream& in,
    std::ostream& out)
{
  std::string_view request = trim(line);
  if (request.empty())
    return Action::Continue;

  bool handled = false;
  Action action = handle_builtin(request, out, handled);
  if (handled)
    return action;

  return handle_request(request, in, out);
}

Shell::Action Shell::handle_builtin(
    std::string_view line,
    std::ostream& out,
    bool& handled)
{
  handled = true;

  if (line == "exit" or line == "quit")
    return Action::Quit;

  if (line == "cd" or line.starts_with("cd ")) {
    std::string_view target = trim(line.substr(2));
    std::string directory;
    if (target.empty()) {
      const char* home = std::getenv("HOME");
      if (home == nullptr) {
        out << "cd: HOME is not set\n";
        return Action::Continue;
      }
      directory = home;
    } else {
      auto words = tokenize(target);
      if (not words.has_value()) {
        out << "cd: " << describe(words.error()) << "\n";
        return Action::Continue;
      }
      if (words->size() != 1) {
        out << "cd: expected exactly one directory\n";
        return Action::Continue;
      }
      directory = words->front();
    }
    // cd has to happen in this process; a child could not change our directory.
    if (::chdir(directory.c_str()) != 0)
      out << "cd: cannot change to " << directory << "\n";
    return Action::Continue;
  }

  handled = false;
  return Action::Continue;
}

Shell::Action Shell::handle_request(
    std::string_view line,
    std::istream& in,
    std::ostream& out)
{
  if (not ensure_model(out))
    return Action::Continue;

  NeedleOptions options;
  options.force_tools = true;

  auto reply = client_.ask(line, kRunCommandTools, options);
  if (not reply.has_value()) {
    out << describe(reply.error()) << "\n";
    return Action::Continue;
  }

  if (reply->calls.empty()) {
    out << reply->text << "\n";
    return Action::Continue;
  }

  auto command = command_from_tool_call(reply->calls.front());
  if (not command.has_value()) {
    out << describe(command.error()) << "\n";
    return Action::Continue;
  }

  out << "> " << render(*command) << "\n";

  if (config_.confirm_risky and is_risky(*command) and
      not confirm(*command, in, out))
    return Action::Continue;

  auto status = execute(*command);
  if (not status.has_value()) {
    out << describe(status.error()) << ": " << command->program << "\n";
    return Action::Continue;
  }
  if (*status != 0)
    out << "[exit " << *status << "]\n";

  return Action::Continue;
}

bool Shell::confirm(
    const Command& command,
    std::istream& in,
    std::ostream& out)
{
  out << render(command) << " looks destructive. run? [y/N] " << std::flush;
  std::string answer;
  if (not std::getline(in, answer))
    return false;
  std::string_view trimmed = trim(answer);
  return trimmed == "y" or trimmed == "Y" or trimmed == "yes";
}

bool Shell::ensure_model(std::ostream& out) {
  if (model_ready_)
    return true;

  if (config_.model_path.empty()) {
    out << "no model: pass a Needle weights directory as the first argument "
           "or set CACTUS_NEEDLE_MODEL\n";
    return false;
  }

  auto loaded = client_.load(config_.model_path);
  if (not loaded.has_value()) {
    out << describe(loaded.error()) << ": " << config_.model_path << "\n";
    return false;
  }

  model_ready_ = true;
  return true;
}

}  // namespace cactus
