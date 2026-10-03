#ifndef NEEDLE_H
#define NEEDLE_H

#include <expected>
#include <string>
#include <string_view>
#include <vector>

#include <needle_ffi.h>

namespace cactus {

enum class NeedleError : int {
  ModelLoadFailed = 0,
  NotLoaded,
  ToolsRejected,
  CompletionFailed,
  ResponseTruncated,
  MalformedResponse,
  ModelReportedError,
};

std::string_view describe(NeedleError error);

struct NeedleOptions {
  int max_tokens = 256;
};

struct ToolCall {
  std::string name;
  std::string arguments;  // raw JSON object as returned by the model
};

// Needle 3 never answers in free text. A turn is a list of calls, possibly
// empty, plus the model's one-line account of how it read the request.
struct NeedleReply {
  std::vector<ToolCall> calls;
  std::string reasoning;
  double confidence = 0.0;
};

// Wraps the Needle 3 engine. The engine holds one process-global model, so
// every client in a process shares it: loading through one client replaces
// the model the others see, and unload() only detaches this client, since
// the engine has no call that frees a model.
//
// Each ask() is an independent turn. The engine would otherwise accumulate
// turns into one conversation, and a shell request should not be read in the
// light of the last one.
struct NeedleClient {
  NeedleClient() = default;

  // system_prompt is session facts such as "device: laptop; os: macOS", not
  // instructions. Needle 3 reads it as facts and ignores directives.
  explicit NeedleClient(std::string system_prompt);

  NeedleClient(NeedleClient&& other) noexcept;
  NeedleClient& operator=(NeedleClient&& other) noexcept;
  NeedleClient(const NeedleClient&) = delete;
  NeedleClient& operator=(const NeedleClient&) = delete;

  // model_path is a .cact file, or a directory holding needle3.cact.
  std::expected<void, NeedleError> load(const std::string& model_path);
  void unload() { mLoaded = false; }
  bool loaded() const { return mLoaded; }

  // tools_json is a JSON array of tool schemas, either Needle's compact form
  // or OpenAI-style wrappers, or empty for no tools.
  std::expected<NeedleReply, NeedleError> ask(
      std::string_view request,
      std::string_view tools_json,
      const NeedleOptions& options);

  std::expected<NeedleReply, NeedleError> ask(std::string_view request) {
    return ask(request, {}, NeedleOptions{});
  }

  void reset();

  static std::expected<NeedleReply, NeedleError> parse_reply(
      std::string_view json);

protected:
  std::expected<void, NeedleError> configure(std::string_view tools_json);

  std::string mSystemPrompt;
  bool mLoaded = false;
  std::vector<char> mBuffer = std::vector<char>(65536);
};

}  // namespace cactus

#endif
