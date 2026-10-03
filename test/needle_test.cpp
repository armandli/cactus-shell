#include <string>

#include <gtest/gtest.h>

#include <needle.h>

namespace {

TEST(NeedleClientOffline, LoadRejectsMissingModel) {
  cactus::NeedleClient client;
  auto loaded = client.load("/nonexistent/needle3.cact");
  ASSERT_FALSE(loaded.has_value());
  EXPECT_EQ(loaded.error(), cactus::NeedleError::ModelLoadFailed);
  EXPECT_FALSE(client.loaded());
}

TEST(NeedleClientOffline, AskWithoutLoadIsNotLoaded) {
  cactus::NeedleClient client;
  auto reply = client.ask("anything");
  ASSERT_FALSE(reply.has_value());
  EXPECT_EQ(reply.error(), cactus::NeedleError::NotLoaded);
}

// Captured from the Needle 3 engine, trimmed of its timing fields.
TEST(NeedleReplyParse, ReadsCallsReasoningAndConfidence) {
  auto reply = cactus::NeedleClient::parse_reply(R"({
    "type": "call", "success": true, "error": null,
    "function_calls": [
      {"name": "list_files", "arguments": {"all": true, "path": "/tmp"}},
      {"name": "print_working_directory", "arguments": {}}
    ],
    "suppressed_calls": [],
    "reasoning": "all=true from 'list all'; path='/tmp' from query",
    "confidence": 0.4808
  })");
  ASSERT_TRUE(reply.has_value()) << cactus::describe(reply.error());
  ASSERT_EQ(reply->calls.size(), 2u);
  EXPECT_EQ(reply->calls[0].name, "list_files");
  EXPECT_EQ(reply->calls[0].arguments, R"({"all":true,"path":"/tmp"})");
  EXPECT_EQ(reply->calls[1].arguments, "{}");
  EXPECT_EQ(reply->reasoning, "all=true from 'list all'; path='/tmp' from query");
  EXPECT_DOUBLE_EQ(reply->confidence, 0.4808);
}

TEST(NeedleReplyParse, SuppressedCallsAreNotRun) {
  auto reply = cactus::NeedleClient::parse_reply(R"({
    "success": true, "function_calls": [],
    "suppressed_calls": [{"name": "rm", "arguments": {"paths": ["a.txt"]}}],
    "reasoning": "rename requested", "confidence": 0.0195
  })");
  ASSERT_TRUE(reply.has_value());
  EXPECT_TRUE(reply->calls.empty());
}

TEST(NeedleReplyParse, ReportsEngineFailure) {
  auto reply = cactus::NeedleClient::parse_reply(
      R"({"success": false, "error": "context overflow"})");
  ASSERT_FALSE(reply.has_value());
  EXPECT_EQ(reply.error(), cactus::NeedleError::ModelReportedError);
}

TEST(NeedleReplyParse, RejectsReplyWithoutSuccessField) {
  auto reply = cactus::NeedleClient::parse_reply(R"({"function_calls": []})");
  ASSERT_FALSE(reply.has_value());
  EXPECT_EQ(reply.error(), cactus::NeedleError::MalformedResponse);
}

TEST(NeedleReplyParse, RejectsCallWithoutName) {
  auto reply = cactus::NeedleClient::parse_reply(
      R"({"success": true, "function_calls": [{"arguments": {}}]})");
  ASSERT_FALSE(reply.has_value());
  EXPECT_EQ(reply.error(), cactus::NeedleError::MalformedResponse);
}

}  // namespace
