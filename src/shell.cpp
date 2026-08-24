#include <shell.h>

#include <cstdlib>

#include <unistd.h>

#include <iostream>
#include <string>

#include <command.h>
#include <needle.h>
#include <tokenize.h>
#include <tools.h>

namespace cactus {

namespace {

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
    if (target.empty()) {
      change_directory({}, out);
      return Action::Continue;
    }
    auto words = tokenize(target);
    if (not words.has_value()) {
      out << "cd: " << describe(words.error()) << "\n";
      return Action::Continue;
    }
    if (words->size() != 1) {
      out << "cd: expected exactly one directory\n";
      return Action::Continue;
    }
    change_directory(words->front(), out);
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

  auto reply = client_.ask(line, tool_catalog_json(), options);
  if (not reply.has_value()) {
    out << describe(reply.error()) << "\n";
    return Action::Continue;
  }

  if (reply->calls.empty()) {
    out << reply->text << "\n";
    return Action::Continue;
  }

  for (const ToolCall& call : reply->calls)
    if (not run_call(call, in, out))
      break;

  return Action::Continue;
}

bool Shell::run_call(
    const ToolCall& call,
    std::istream& in,
    std::ostream& out)
{
  auto command = command_from_tool_call(call);
  if (not command.has_value()) {
    out << describe(command.error()) << ": " << call.name << "\n";
    return false;
  }

  out << "> " << render(*command) << "\n";

  if (command->risky and config_.confirm_risky and
      not confirm(*command, in, out))
    return false;

  if (command->in_process)
    return change_directory(command->args.empty() ? std::string{}
                                                  : command->args.front(),
                            out);

  auto status = execute(*command);
  if (not status.has_value()) {
    out << describe(status.error()) << ": " << command->program << "\n";
    return false;
  }
  if (*status != 0) {
    out << "[exit " << *status << "]\n";
    return false;
  }
  return true;
}

bool Shell::change_directory(const std::string& path, std::ostream& out) {
  // cd has to happen in this process; a child could not move the shell.
  std::string target = path;
  if (target.empty()) {
    const char* home = std::getenv("HOME");
    if (home == nullptr) {
      out << "cd: HOME is not set\n";
      return false;
    }
    target = home;
  }
  if (::chdir(target.c_str()) != 0) {
    out << "cd: cannot change to " << target << "\n";
    return false;
  }
  return true;
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
