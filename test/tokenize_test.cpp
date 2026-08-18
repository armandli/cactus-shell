#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <tokenize.h>

namespace {

using Words = std::vector<std::string>;

Words split(std::string_view line) {
  auto words = cactus::tokenize(line);
  EXPECT_TRUE(words.has_value())
      << cactus::describe(words.error());
  if (not words.has_value())
    return {};
  return *words;
}

TEST(TokenizeTest, splits_on_single_spaces) {
  EXPECT_EQ(split("ls -la /tmp"), (Words{"ls", "-la", "/tmp"}));
}

TEST(TokenizeTest, collapses_repeated_whitespace) {
  EXPECT_EQ(split("  ls \t\t -la   /tmp  "), (Words{"ls", "-la", "/tmp"}));
}

TEST(TokenizeTest, empty_line_yields_no_words) {
  EXPECT_TRUE(split("").empty());
}

TEST(TokenizeTest, whitespace_only_line_yields_no_words) {
  EXPECT_TRUE(split("   \t  ").empty());
}

TEST(TokenizeTest, single_quotes_are_literal) {
  EXPECT_EQ(split(R"(echo 'a b  c')"), (Words{"echo", "a b  c"}));
}

TEST(TokenizeTest, single_quotes_do_not_honour_escapes) {
  EXPECT_EQ(split(R"(echo 'a\nb')"), (Words{"echo", R"(a\nb)"}));
}

TEST(TokenizeTest, double_quotes_keep_spaces) {
  EXPECT_EQ(split(R"(grep "hello world" file)"),
            (Words{"grep", "hello world", "file"}));
}

TEST(TokenizeTest, double_quotes_honour_escaped_quote) {
  EXPECT_EQ(split(R"(echo "say \"hi\"")"), (Words{"echo", R"(say "hi")"}));
}

TEST(TokenizeTest, double_quotes_honour_escaped_backslash) {
  EXPECT_EQ(split(R"(echo "a\\b")"), (Words{"echo", R"(a\b)"}));
}

TEST(TokenizeTest, other_backslashes_survive_inside_double_quotes) {
  EXPECT_EQ(split(R"(echo "a\nb")"), (Words{"echo", R"(a\nb)"}));
}

TEST(TokenizeTest, adjacent_fragments_join_into_one_word) {
  EXPECT_EQ(split(R"(pre'mid dle'post)"), (Words{"premid dlepost"}));
}

TEST(TokenizeTest, empty_quotes_produce_an_empty_word) {
  EXPECT_EQ(split(R"(echo '' "")"), (Words{"echo", "", ""}));
}

TEST(TokenizeTest, bare_backslash_escapes_next_character) {
  EXPECT_EQ(split(R"(ls a\ b)"), (Words{"ls", "a b"}));
}

TEST(TokenizeTest, bare_backslash_escapes_a_quote) {
  EXPECT_EQ(split(R"(echo \'x)"), (Words{"echo", "'x"}));
}

TEST(TokenizeTest, unterminated_single_quote_is_an_error) {
  auto words = cactus::tokenize("echo 'oops");
  ASSERT_FALSE(words.has_value());
  EXPECT_EQ(words.error(), cactus::TokenError::UnterminatedQuote);
}

TEST(TokenizeTest, unterminated_double_quote_is_an_error) {
  auto words = cactus::tokenize(R"(echo "oops)");
  ASSERT_FALSE(words.has_value());
  EXPECT_EQ(words.error(), cactus::TokenError::UnterminatedQuote);
}

TEST(TokenizeTest, trailing_backslash_is_an_error) {
  auto words = cactus::tokenize("echo hi\\");
  ASSERT_FALSE(words.has_value());
  EXPECT_EQ(words.error(), cactus::TokenError::TrailingEscape);
}

TEST(TokenizeTest, every_error_has_a_description) {
  EXPECT_FALSE(cactus::describe(cactus::TokenError::UnterminatedQuote).empty());
  EXPECT_FALSE(cactus::describe(cactus::TokenError::TrailingEscape).empty());
}

}  // namespace
