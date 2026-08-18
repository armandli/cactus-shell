#include <tokenize.h>

#include <cassert>

namespace cactus {

std::string_view describe(TokenError error) {
  switch (error) {
    case TokenError::UnterminatedQuote:
      return "unterminated quote";

    break; case TokenError::TrailingEscape:
      return "command line ends with a dangling backslash";

    break; default:
      assert(false); // should never get here
      return "unknown tokenizer error";
  }
}

std::expected<std::vector<std::string>, TokenError> tokenize(
    std::string_view line)
{
  std::vector<std::string> words;
  std::string current;
  bool started = false;

  for (std::size_t i = 0; i < line.size(); ++i) {
    char c = line[i];

    if (c == ' ' or c == '\t' or c == '\n' or c == '\r') {
      if (started) {
        words.push_back(current);
        current.clear();
        started = false;
      }
      continue;
    }

    started = true;

    if (c == '\'') {
      std::size_t end = line.find('\'', i + 1);
      if (end == std::string_view::npos)
        return std::unexpected(TokenError::UnterminatedQuote);
      current.append(line.substr(i + 1, end - i - 1));
      i = end;
      continue;
    }

    if (c == '"') {
      ++i;
      bool closed = false;
      for (; i < line.size(); ++i) {
        char q = line[i];
        if (q == '"') {
          closed = true;
          break;
        }
        if (q == '\\' and i + 1 < line.size() and
            (line[i + 1] == '"' or line[i + 1] == '\\')) {
          ++i;
          current.push_back(line[i]);
          continue;
        }
        current.push_back(q);
      }
      if (not closed)
        return std::unexpected(TokenError::UnterminatedQuote);
      continue;
    }

    if (c == '\\') {
      if (i + 1 >= line.size())
        return std::unexpected(TokenError::TrailingEscape);
      ++i;
      current.push_back(line[i]);
      continue;
    }

    current.push_back(c);
  }

  if (started)
    words.push_back(current);

  return words;
}

}  // namespace cactus
