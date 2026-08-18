#include <cstdint>

#include <string>

#include <gtest/gtest.h>

#include <json_util.h>

namespace {

std::string build(cactus::JsonBuilder& builder) {
  auto rendered = builder.str();
  EXPECT_TRUE(rendered.has_value());
  return rendered.value_or(std::string{});
}

TEST(JsonBuilder, EmptyObject) {
  cactus::JsonBuilder builder;
  builder.begin_object().end_object();
  EXPECT_EQ(build(builder), "{}");
}

TEST(JsonBuilder, EmptyArray) {
  cactus::JsonBuilder builder;
  builder.begin_array().end_array();
  EXPECT_EQ(build(builder), "[]");
}

TEST(JsonBuilder, FlatObjectSeparatesFields) {
  cactus::JsonBuilder builder;
  builder.begin_object()
      .field("name", std::string_view{"ls"})
      .field("count", std::int64_t{2})
      .field("ok", true)
      .end_object();
  EXPECT_EQ(build(builder), R"({"name":"ls","count":2,"ok":true})");
}

TEST(JsonBuilder, ArrayOfScalarsSeparatesElements) {
  cactus::JsonBuilder builder;
  builder.begin_array()
      .value(std::int64_t{1})
      .value(std::int64_t{2})
      .value(std::int64_t{3})
      .end_array();
  EXPECT_EQ(build(builder), "[1,2,3]");
}

TEST(JsonBuilder, NestedContainers) {
  cactus::JsonBuilder builder;
  builder.begin_object()
      .key("args")
      .begin_array()
      .value(std::string_view{"-l"})
      .value(std::string_view{"-a"})
      .end_array()
      .field("shell", std::string_view{"cactus"})
      .end_object();
  EXPECT_EQ(build(builder), R"({"args":["-l","-a"],"shell":"cactus"})");
}

TEST(JsonBuilder, EscapesQuotesAndBackslashes) {
  cactus::JsonBuilder builder;
  builder.begin_object()
      .field("path", std::string_view{R"(C:\a "b")"})
      .end_object();
  EXPECT_EQ(build(builder), R"({"path":"C:\\a \"b\""})");
}

TEST(JsonBuilder, EscapesControlCharacters) {
  cactus::JsonBuilder builder;
  builder.begin_object().field("text", std::string_view{"a\nb\tc"}).end_object();
  EXPECT_EQ(build(builder), R"({"text":"a\nb\tc"})");
}

TEST(JsonBuilder, NullValue) {
  cactus::JsonBuilder builder;
  builder.begin_object().key("error").null_value().end_object();
  EXPECT_EQ(build(builder), R"({"error":null})");
}

TEST(JsonBuilder, RawValueIsNotEscaped) {
  cactus::JsonBuilder builder;
  builder.begin_object().key("inner").raw_value(R"({"a":1})").end_object();
  EXPECT_EQ(build(builder), R"({"inner":{"a":1}})");
}

TEST(JsonBuilder, UnbalancedIsRejected) {
  cactus::JsonBuilder builder;
  builder.begin_object().field("a", std::int64_t{1});
  EXPECT_FALSE(builder.balanced());
  auto rendered = builder.str();
  ASSERT_FALSE(rendered.has_value());
  EXPECT_EQ(rendered.error(), cactus::JsonError::Unbalanced);
}

TEST(JsonBuilder, ClearResetsState) {
  cactus::JsonBuilder builder;
  builder.begin_object().field("a", std::int64_t{1}).end_object();
  builder.clear();
  EXPECT_TRUE(builder.balanced());
  builder.begin_array().end_array();
  EXPECT_EQ(build(builder), "[]");
}

TEST(JsonBuilder, RoundTripsThroughParser) {
  cactus::JsonBuilder builder;
  builder.begin_object()
      .field("cmd", std::string_view{"echo hi"})
      .field("timeout", std::int64_t{30})
      .end_object();

  auto doc = cactus::JsonDoc::parse(build(builder));
  ASSERT_TRUE(doc.has_value());
  EXPECT_EQ(cactus::json_string(doc->root(), "cmd").value_or(""), "echo hi");
  EXPECT_EQ(cactus::json_int(doc->root(), "timeout").value_or(0), 30);
}

TEST(JsonDoc, ParsesObjectFields) {
  auto doc = cactus::JsonDoc::parse(
      R"({"s":"txt","i":-7,"d":2.5,"b":false})");
  ASSERT_TRUE(doc.has_value());

  EXPECT_EQ(cactus::json_string(doc->root(), "s").value_or(""), "txt");
  EXPECT_EQ(cactus::json_int(doc->root(), "i").value_or(0), -7);
  EXPECT_DOUBLE_EQ(cactus::json_double(doc->root(), "d").value_or(0.0), 2.5);
  EXPECT_EQ(cactus::json_bool(doc->root(), "b").value_or(true), false);
}

TEST(JsonDoc, ParsesNestedArray) {
  auto doc = cactus::JsonDoc::parse(R"({"xs":[10,20,30]})");
  ASSERT_TRUE(doc.has_value());

  auto xs = cactus::json_array(doc->root(), "xs");
  ASSERT_TRUE(xs.has_value());
  EXPECT_EQ(xs->size(), 3u);

  std::int64_t total = 0;
  for (auto item : xs.value()) {
    std::int64_t parsed = 0;
    ASSERT_FALSE(item.get(parsed));
    total += parsed;
  }
  EXPECT_EQ(total, 60);
}

TEST(JsonDoc, ParsesNestedObject) {
  auto doc = cactus::JsonDoc::parse(R"({"outer":{"inner":"deep"}})");
  ASSERT_TRUE(doc.has_value());

  auto outer = cactus::json_object(doc->root(), "outer");
  ASSERT_TRUE(outer.has_value());
  EXPECT_EQ(cactus::json_string(doc->root()["outer"].value(), "inner")
                .value_or(""),
            "deep");
}

TEST(JsonDoc, RejectsMalformedInput) {
  auto doc = cactus::JsonDoc::parse(R"({"a":)");
  ASSERT_FALSE(doc.has_value());
  EXPECT_EQ(doc.error(), cactus::JsonError::ParseFailed);
}

TEST(JsonDoc, RejectsEmptyInput) {
  auto doc = cactus::JsonDoc::parse("");
  ASSERT_FALSE(doc.has_value());
  EXPECT_EQ(doc.error(), cactus::JsonError::ParseFailed);
}

TEST(JsonDoc, MissingKeyIsDistinctFromTypeMismatch) {
  auto doc = cactus::JsonDoc::parse(R"({"a":"text"})");
  ASSERT_TRUE(doc.has_value());

  auto missing = cactus::json_string(doc->root(), "nope");
  ASSERT_FALSE(missing.has_value());
  EXPECT_EQ(missing.error(), cactus::JsonError::KeyNotFound);

  auto wrong_type = cactus::json_int(doc->root(), "a");
  ASSERT_FALSE(wrong_type.has_value());
  EXPECT_EQ(wrong_type.error(), cactus::JsonError::TypeMismatch);
}

TEST(JsonDoc, SurvivesSourceBufferGoingOutOfScope) {
  cactus::JsonDoc doc;
  {
    std::string scratch = R"({"k":"v"})";
    auto parsed = cactus::JsonDoc::parse(scratch);
    ASSERT_TRUE(parsed.has_value());
    doc = std::move(parsed.value());
    scratch.assign(scratch.size(), 'x');
  }
  EXPECT_EQ(cactus::json_string(doc.root(), "k").value_or(""), "v");
}

TEST(JsonError, DescribeIsNonEmptyForEveryError) {
  EXPECT_FALSE(cactus::describe(cactus::JsonError::ParseFailed).empty());
  EXPECT_FALSE(cactus::describe(cactus::JsonError::KeyNotFound).empty());
  EXPECT_FALSE(cactus::describe(cactus::JsonError::TypeMismatch).empty());
  EXPECT_FALSE(cactus::describe(cactus::JsonError::Unbalanced).empty());
}

}  // namespace
