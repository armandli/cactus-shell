#ifndef NEEDLE_H
#define NEEDLE_H

#include <cstdint>

#include <expected>
#include <string>
#include <string_view>
#include <vector>

#include <needle_ffi.h>

namespace cactus {

enum class NeedleError : int {
  ModelLoadFailed = 0,
  NotLoaded,
  CompletionFailed,
  ResponseTruncated,
  MalformedResponse,
  ModelReportedError,
};

std::string_view describe(NeedleError error);

struct NeedleOptions {
  std::int64_t max_tokens = 256;
  double temperature = 0.0;
  double top_p = 0.0;
  std::int64_t top_k = 0;
  bool force_tools = false;
  bool auto_handoff = false;
};

struct ToolCall {
  std::string name;
  std::string arguments;  // raw JSON object as returned by the model
};

struct NeedleReply {
  std::string text;
  std::vector<ToolCall> calls;
  double confidence = 0.0;
  bool cloud_handoff = false;
};

// Wraps one loaded cactus model. Feeds natural-language input to the model as
// chat JSON and returns the parsed JSON reply.
struct NeedleClient {
  NeedleClient() = default;
  explicit NeedleClient(std::string system_prompt);
  ~NeedleClient();

  NeedleClient(NeedleClient&& other) noexcept;
  NeedleClient& operator=(NeedleClient&& other) noexcept;
  NeedleClient(const NeedleClient&) = delete;
  NeedleClient& operator=(const NeedleClient&) = delete;

  std::expected<void, NeedleError> load(const std::string& model_path);
  void unload();
  bool loaded() const { return model_ != nullptr; }

  // tools_json is an OpenAI-style tool array, or empty for a plain completion.
  std::expected<NeedleReply, NeedleError> ask(
      std::string_view request,
      std::string_view tools_json,
      const NeedleOptions& options);

  std::expected<NeedleReply, NeedleError> ask(std::string_view request) {
    return ask(request, {}, NeedleOptions{});
  }

  void reset();

  std::string render_messages(std::string_view request) const;
  static std::string render_options(const NeedleOptions& options);
  static std::expected<NeedleReply, NeedleError> parse_reply(
      std::string_view json);

protected:
  std::string system_prompt_;
  cactus_model_t model_ = nullptr;
  std::vector<char> buffer_ = std::vector<char>(16384);
};

}  // namespace cactus

#endif
