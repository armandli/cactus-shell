#include <tools.h>

#include <cassert>

#include <algorithm>
#include <array>

#include <json_util.h>

namespace cactus {

namespace {

constexpr std::array kLsParams = {
    ToolParam{"long", "Show permissions, owner, size and date for each entry",
              ParamKind::Flag, "-l", false},
    ToolParam{"all", "Include entries whose names begin with a dot",
              ParamKind::Flag, "-a", false},
    ToolParam{"recursive", "Descend into subdirectories",
              ParamKind::Flag, "-R", false},
    ToolParam{"path", "Directory or file to list; defaults to the current "
              "directory when omitted",
              ParamKind::Positional, "", false},
};

constexpr std::array kCdParams = {
    ToolParam{"path", "Directory to move to; defaults to the home directory "
              "when omitted",
              ParamKind::Positional, "", false},
};

constexpr std::array kCatParams = {
    ToolParam{"paths", "Files to print, in order",
              ParamKind::PositionalList, "", true},
};

constexpr std::array kHeadParams = {
    ToolParam{"lines", "How many lines to show from the top; the program's "
              "default is 10",
              ParamKind::Number, "-n", false},
    ToolParam{"path", "File to read", ParamKind::Positional, "", true},
};

constexpr std::array kTailParams = {
    ToolParam{"lines", "How many lines to show from the end; the program's "
              "default is 10",
              ParamKind::Number, "-n", false},
    ToolParam{"path", "File to read", ParamKind::Positional, "", true},
};

constexpr std::array kGrepParams = {
    ToolParam{"ignore_case", "Match without regard to upper or lower case",
              ParamKind::Flag, "-i", false},
    ToolParam{"recursive", "Search every file under the given directories",
              ParamKind::Flag, "-r", false},
    ToolParam{"line_numbers", "Prefix each match with its line number",
              ParamKind::Flag, "-n", false},
    ToolParam{"pattern", "Text or regular expression to look for",
              ParamKind::Positional, "", true},
    ToolParam{"paths", "Files or directories to search",
              ParamKind::PositionalList, "", true},
};

constexpr std::array kFindParams = {
    ToolParam{"path", "Directory to search under",
              ParamKind::Positional, "", true},
    ToolParam{"name", "Filename pattern to match, such as *.cpp",
              ParamKind::Option, "-name", false},
};

constexpr std::array kWcParams = {
    ToolParam{"lines", "Count lines", ParamKind::Flag, "-l", false},
    ToolParam{"words", "Count words", ParamKind::Flag, "-w", false},
    ToolParam{"bytes", "Count bytes", ParamKind::Flag, "-c", false},
    ToolParam{"paths", "Files to measure", ParamKind::PositionalList, "", true},
};

constexpr std::array kWhichParams = {
    ToolParam{"program", "Name of the program to locate",
              ParamKind::Positional, "", true},
};

constexpr std::array kFileParams = {
    ToolParam{"paths", "Files whose type should be identified",
              ParamKind::PositionalList, "", true},
};

constexpr std::array kDuParams = {
    ToolParam{"human", "Report sizes as KB, MB and GB rather than blocks",
              ParamKind::Flag, "-h", false},
    ToolParam{"summarize", "Report one total instead of every subdirectory",
              ParamKind::Flag, "-s", false},
    ToolParam{"path", "File or directory to measure; defaults to the current "
              "directory when omitted",
              ParamKind::Positional, "", false},
};

constexpr std::array kDfParams = {
    ToolParam{"human", "Report sizes as KB, MB and GB rather than blocks",
              ParamKind::Flag, "-h", false},
    ToolParam{"path", "Limit the report to the filesystem holding this path",
              ParamKind::Positional, "", false},
};

constexpr std::array kPsParams = {
    ToolParam{"all", "Include processes belonging to every user",
              ParamKind::Flag, "-e", false},
    ToolParam{"full", "Show the full command line of each process",
              ParamKind::Flag, "-f", false},
};

constexpr std::array kEchoParams = {
    ToolParam{"words", "Words to print, separated by spaces",
              ParamKind::PositionalList, "", true},
};

constexpr std::array kSortParams = {
    ToolParam{"reverse", "Sort from largest to smallest",
              ParamKind::Flag, "-r", false},
    ToolParam{"numeric", "Compare as numbers rather than as text",
              ParamKind::Flag, "-n", false},
    ToolParam{"paths", "Files whose lines should be sorted",
              ParamKind::PositionalList, "", true},
};

constexpr std::array kMkdirParams = {
    ToolParam{"parents", "Create any missing parent directories too, and do "
              "not fail if the directory already exists",
              ParamKind::Flag, "-p", false},
    ToolParam{"paths", "Directories to create",
              ParamKind::PositionalList, "", true},
};

constexpr std::array kRmdirParams = {
    ToolParam{"paths", "Empty directories to remove",
              ParamKind::PositionalList, "", true},
};

constexpr std::array kRmParams = {
    ToolParam{"recursive", "Delete directories and everything inside them",
              ParamKind::Flag, "-r", false},
    ToolParam{"force", "Do not complain about files that do not exist",
              ParamKind::Flag, "-f", false},
    ToolParam{"paths", "Files or directories to delete",
              ParamKind::PositionalList, "", true},
};

constexpr std::array kMvParams = {
    ToolParam{"sources", "Files or directories to move",
              ParamKind::PositionalList, "", true},
    ToolParam{"destination", "New name, or the directory to move them into",
              ParamKind::Positional, "", true},
};

constexpr std::array kCpParams = {
    ToolParam{"recursive", "Copy directories and everything inside them",
              ParamKind::Flag, "-r", false},
    ToolParam{"sources", "Files or directories to copy",
              ParamKind::PositionalList, "", true},
    ToolParam{"destination", "New name, or the directory to copy them into",
              ParamKind::Positional, "", true},
};

constexpr std::array kTouchParams = {
    ToolParam{"paths", "Files to create, or whose timestamp to update",
              ParamKind::PositionalList, "", true},
};

constexpr std::array kChmodParams = {
    ToolParam{"recursive", "Apply to everything inside the given directories",
              ParamKind::Flag, "-R", false},
    ToolParam{"mode", "Permissions to set, such as 644 or u+x",
              ParamKind::Positional, "", true},
    ToolParam{"paths", "Files or directories to change",
              ParamKind::PositionalList, "", true},
};

constexpr std::array kKillParams = {
    ToolParam{"signal", "Signal to send, such as TERM or KILL; defaults to "
              "TERM when omitted",
              ParamKind::Option, "-s", false},
    ToolParam{"pid", "Numeric process id to signal",
              ParamKind::Positional, "", true},
};

constexpr std::array<ToolParam, 0> kNoParams = {};

constexpr std::array kCatalog = {
    ToolSpec{"ls", "ls", "List the files and directories at a path",
             kLsParams, false, false},
    ToolSpec{"pwd", "pwd", "Print the directory the shell is currently in",
             kNoParams, false, false},
    ToolSpec{"cd", "cd", "Change the directory the shell is working in",
             kCdParams, false, true},
    ToolSpec{"cat", "cat", "Print the entire contents of one or more files",
             kCatParams, false, false},
    ToolSpec{"head", "head", "Print the first lines of a file",
             kHeadParams, false, false},
    ToolSpec{"tail", "tail", "Print the last lines of a file",
             kTailParams, false, false},
    ToolSpec{"grep", "grep", "Search files for lines matching a pattern",
             kGrepParams, false, false},
    ToolSpec{"find", "find", "Search a directory tree for files by name",
             kFindParams, false, false},
    ToolSpec{"wc", "wc", "Count the lines, words or bytes in files",
             kWcParams, false, false},
    ToolSpec{"which", "which", "Show the full path of a program on PATH",
             kWhichParams, false, false},
    ToolSpec{"file", "file", "Identify what kind of data a file holds",
             kFileParams, false, false},
    ToolSpec{"du", "du", "Report how much disk space files and directories use",
             kDuParams, false, false},
    ToolSpec{"df", "df", "Report free and used space on mounted filesystems",
             kDfParams, false, false},
    ToolSpec{"ps", "ps", "List processes running on this machine",
             kPsParams, false, false},
    ToolSpec{"date", "date", "Print the current date and time",
             kNoParams, false, false},
    ToolSpec{"whoami", "whoami", "Print the name of the current user",
             kNoParams, false, false},
    ToolSpec{"echo", "echo", "Print text back to the screen",
             kEchoParams, false, false},
    ToolSpec{"sort", "sort", "Print the lines of files in sorted order",
             kSortParams, false, false},
    ToolSpec{"mkdir", "mkdir", "Create one or more directories",
             kMkdirParams, true, false},
    ToolSpec{"rmdir", "rmdir", "Remove directories that are already empty",
             kRmdirParams, true, false},
    ToolSpec{"rm", "rm", "Delete files or directories permanently",
             kRmParams, true, false},
    ToolSpec{"mv", "mv", "Move or rename files and directories",
             kMvParams, true, false},
    ToolSpec{"cp", "cp", "Copy files and directories",
             kCpParams, true, false},
    ToolSpec{"touch", "touch",
             "Create empty files, or update the timestamp of existing ones",
             kTouchParams, true, false},
    ToolSpec{"chmod", "chmod", "Change the permissions on files or directories",
             kChmodParams, true, false},
    ToolSpec{"kill", "kill", "Send a signal to a running process",
             kKillParams, true, false},
};

std::string_view json_type(ParamKind kind) {
  switch (kind) {
    case ParamKind::Flag:
      return "boolean";

    break; case ParamKind::Option:
      return "string";

    break; case ParamKind::Number:
      return "integer";

    break; case ParamKind::Positional:
      return "string";

    break; case ParamKind::PositionalList:
      return "array";

    break; default:
      assert(false); // should never get here
      return "string";
  }
}

std::string render_catalog() {
  JsonBuilder builder;
  builder.begin_array();

  for (const ToolSpec& spec : kCatalog) {
    builder.begin_object()
        .field("type", std::string_view{"function"})
        .key("function")
        .begin_object()
        .field("name", spec.name)
        .field("description", spec.description)
        .key("parameters")
        .begin_object()
        .field("type", std::string_view{"object"})
        .key("properties")
        .begin_object();

    for (const ToolParam& param : spec.params) {
      builder.key(param.name)
          .begin_object()
          .field("type", json_type(param.kind))
          .field("description", param.description);
      if (param.kind == ParamKind::PositionalList) {
        builder.key("items")
            .begin_object()
            .field("type", std::string_view{"string"})
            .end_object();
      }
      builder.end_object();
    }

    builder.end_object().key("required").begin_array();
    for (const ToolParam& param : spec.params)
      if (param.required)
        builder.value(param.name);
    builder.end_array();

    builder.end_object().end_object().end_object();
  }

  builder.end_array();
  return builder.str().value_or(std::string{"[]"});
}

}  // namespace

std::span<const ToolSpec> tool_catalog() {
  return kCatalog;
}

const ToolSpec* find_tool(std::string_view name) {
  auto found = std::ranges::find(kCatalog, name, &ToolSpec::name);
  return found == kCatalog.end() ? nullptr : &*found;
}

const std::string& tool_catalog_json() {
  static const std::string rendered = render_catalog();
  return rendered;
}

}  // namespace cactus
