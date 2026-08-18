#include <cstdlib>

#include <string>

#include <gtest/gtest.h>

#include <needle.h>

// These are integration tests: they require a real cactus build and a Needle
// weights directory. Point CACTUS_NEEDLE_MODEL at that directory to run them;
// without it every case reports as skipped rather than failed.

namespace {

const char* model_path() {
  return std::getenv("CACTUS_NEEDLE_MODEL");
}

std::string weather_tool() {
  return R"([{
    "type": "function",
    "function": {
      "name": "run_command",
      "description": "Run a shell command",
      "parameters": {
        "type": "object",
        "properties": {
          "command": {"type": "string", "description": "The command to run"}
        },
        "required": ["command"]
      }
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

  cactus::NeedleClient client{
      "Translate the user request into a shell command."};
};

TEST_F(NeedleLive, LoadsModel) {
  EXPECT_TRUE(client.loaded());
}

TEST_F(NeedleLive, AnswersPlainPrompt) {
  auto reply = client.ask("Reply with the single word: ready");
  ASSERT_TRUE(reply.has_value()) << cactus::describe(reply.error());
  EXPECT_FALSE(reply->text.empty());
}

TEST_F(NeedleLive, ProducesToolCallForCommandRequest) {
  cactus::NeedleOptions options;
  options.force_tools = true;
  options.max_tokens = 128;

  auto reply = client.ask("list the files in this directory",
                          weather_tool(),
                          options);
  ASSERT_TRUE(reply.has_value()) << cactus::describe(reply.error());
  ASSERT_FALSE(reply->calls.empty());
  EXPECT_EQ(reply->calls.front().name, "run_command");
  EXPECT_FALSE(reply->calls.front().arguments.empty());
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

TEST(NeedleClientOffline, LoadRejectsMissingModelDirectory) {
  if (model_path() == nullptr) {
    GTEST_SKIP() << "set CACTUS_NEEDLE_MODEL to run Needle integration tests";
  }
  cactus::NeedleClient client;
  auto loaded = client.load("/nonexistent/needle/weights");
  ASSERT_FALSE(loaded.has_value());
  EXPECT_EQ(loaded.error(), cactus::NeedleError::ModelLoadFailed);
}

}  // namespace
