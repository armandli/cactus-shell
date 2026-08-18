#ifndef TOKENIZE_H
#define TOKENIZE_H

#include <expected>
#include <string>
#include <string_view>
#include <vector>

namespace cactus {

enum class TokenError : int {
  UnterminatedQuote = 0,
  TrailingEscape,
};

std::string_view describe(TokenError error);

// Splits a command line into argv the way a shell would, without invoking one:
// unquoted whitespace separates words, '...' is literal, "..." honours \" and
// \\, and a bare backslash escapes the next character.
std::expected<std::vector<std::string>, TokenError> tokenize(
    std::string_view line);

}  // namespace cactus

#endif
