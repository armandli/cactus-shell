#include <cstdlib>

#include <string>

#include <gtest/gtest.h>

#include <needle.h>

// The NeedleLive cases are integration tests: they need the Needle 3 model.
// Point CACTUS_NEEDLE_MODEL at needle3.cact (or the directory holding it) to
// run them; without it they report as skipped rather than failed.

namespace {

const char* model_path() {
  return std::getenv("CACTUS_NEEDLE_MODEL");
}

// One narrow tool, the shape the shell sends. Needle 3 fills every argument
// from a span of the request, so a free-form "run this command" tool gets no
// call at all: no span of a request is a shell command.
std::string list_files_tool() {
  return R"([{
    "name": "list_files",
    "description": "List the files and directories at a path",
    "parameters": {
      "type": "object",
      "properties": {
        "path": {"type": "string", "description": "Directory to list"}
      },
      "required": []
    }
  }])";
}

struct NeedleLive : public ::testing::Test {
protected:
  void SetUp() override {
    const char* path = model_path();
    if (path == nullptr) {
      GTEST_SKIP() << "set CACTUS_NEEDLE_MODEL to run Needle integration tests";
    }
    auto loaded = client.load(std::string(path));
    ASSERT_TRUE(loaded.has_value())
        << cactus::describe(loaded.error());
  }

  cactus::NeedleClient client;
};

TEST_F(NeedleLive, LoadsModel) {
  EXPECT_TRUE(client.loaded());
}

TEST_F(NeedleLive, ReturnsNoCallsWithoutTools) {
  auto reply = client.ask("Reply with the single word: ready");
  ASSERT_TRUE(reply.has_value()) << cactus::describe(reply.error());
  EXPECT_TRUE(reply->calls.empty());
  EXPECT_FALSE(reply->reasoning.empty());
}

TEST_F(NeedleLive, ProducesToolCallForCommandRequest) {
  cactus::NeedleOptions options;
  options.max_tokens = 128;

  auto reply = client.ask("list the files in /tmp", list_files_tool(), options);
  ASSERT_TRUE(reply.has_value()) << cactus::describe(reply.error());
  ASSERT_EQ(reply->calls.size(), 1u) << reply->reasoning;
  EXPECT_EQ(reply->calls.front().name, "list_files");
  EXPECT_NE(reply->calls.front().arguments.find("/tmp"), std::string::npos)
      << reply->calls.front().arguments;
}

TEST_F(NeedleLive, SwitchesBetweenToolSets) {
  auto with_tools = client.ask("list the files in /tmp",
                               list_files_tool(),
                               cactus::NeedleOptions{});
  ASSERT_TRUE(with_tools.has_value());
  EXPECT_FALSE(with_tools->calls.empty());

  auto without_tools = client.ask("list the files in /tmp");
  ASSERT_TRUE(without_tools.has_value());
  EXPECT_TRUE(without_tools->calls.empty());
}

TEST_F(NeedleLive, ResetKeepsModelUsable) {
  client.reset();
  EXPECT_TRUE(client.loaded());
  auto reply = client.ask("say hello");
  EXPECT_TRUE(reply.has_value()) << cactus::describe(reply.error());
}

TEST_F(NeedleLive, UnloadReleasesModel) {
  client.unload();
  EXPECT_FALSE(client.loaded());
  auto reply = client.ask("anything");
  ASSERT_FALSE(reply.has_value());
  EXPECT_EQ(reply.error(), cactus::NeedleError::NotLoaded);
}

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
