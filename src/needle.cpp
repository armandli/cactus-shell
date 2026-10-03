#include <needle.h>

#include <cassert>
#include <cstring>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <json_util.h>

namespace cactus {

namespace sj = simdjson;

namespace {

// The system prompt and tools the engine was last initialised with. The
// engine's state is process-global, so this mirror is too: it lets a client
// skip needle_init (which embeds every tool on a cold call) when nothing
// changed, and notices when another client changed it underneath.
std::optional<std::string> gConfiguredPrefix;

std::string prefix_key(std::string_view system_prompt,
                       std::string_view tools_json)
{
  std::string key(system_prompt);
  key.push_back('\0');
  key.append(tools_json);
  return key;
}

}  // namespace

std::string_view describe(NeedleError error) {
  switch (error) {
    case NeedleError::ModelLoadFailed: return "could not load model";

    break; case NeedleError::NotLoaded: return "no model loaded";

    break; case NeedleError::ToolsRejected:
      return "model rejected the tool schemas";

    break; case NeedleError::CompletionFailed: return "completion call failed";

    break; case NeedleError::ResponseTruncated:
      return "response exceeded buffer";

    break; case NeedleError::MalformedResponse:
      return "model returned malformed JSON";

    break; case NeedleError::ModelReportedError:
      return "model reported an error";

    break; default: assert(false); // should never get here
  }
  return "unknown error";
}

NeedleClient::NeedleClient(std::string system_prompt)
  : mSystemPrompt(std::move(system_prompt)) {
}

NeedleClient::NeedleClient(NeedleClient&& other) noexcept
  : mSystemPrompt(std::move(other.mSystemPrompt)),
    mLoaded(other.mLoaded),
    mBuffer(std::move(other.mBuffer)) {
  other.mLoaded = false;
}

NeedleClient& NeedleClient::operator=(NeedleClient&& other) noexcept {
  if (this != &other) {
    mSystemPrompt = std::move(other.mSystemPrompt);
    mLoaded = other.mLoaded;
    mBuffer = std::move(other.mBuffer);
    other.mLoaded = false;
  }
  return *this;
}

std::expected<void, NeedleError> NeedleClient::load(
    const std::string& model_path)
{
  unload();

  std::filesystem::path path(model_path);
  std::error_code ec;
  if (std::filesystem::is_directory(path, ec))
    path /= "needle3.cact";

  std::ifstream file(path, std::ios::binary);
  if (not file)
    return std::unexpected(NeedleError::ModelLoadFailed);
  const std::vector<unsigned char> bytes(
      (std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

  // The engine copies what it needs, so the bytes can go once this returns.
  if (needle_load(bytes.data(), bytes.size()) < 0)
    return std::unexpected(NeedleError::ModelLoadFailed);

  gConfiguredPrefix.reset();
  mLoaded = true;
  return {};
}

void NeedleClient::reset() {
  if (mLoaded)
    needle_reset();
}

std::expected<void, NeedleError> NeedleClient::configure(
    std::string_view tools_json)
{
  std::string key = prefix_key(mSystemPrompt, tools_json);
  if (gConfiguredPrefix == key)
    return {};

  const std::string tools(tools_json);
  const int status = needle_init(
      mSystemPrompt.empty() ? nullptr : mSystemPrompt.c_str(),
      tools.empty() ? nullptr : tools.c_str(),
      nullptr);
  if (status < 0) {
    gConfiguredPrefix.reset();
    return std::unexpected(NeedleError::ToolsRejected);
  }

  gConfiguredPrefix = std::move(key);
  return {};
}

std::expected<NeedleReply, NeedleError> NeedleClient::parse_reply(
    std::string_view json)
{
  auto doc = JsonDoc::parse(json);
  if (not doc) {
    return std::unexpected(NeedleError::MalformedResponse);
  }
  sj::dom::element root = doc->root();

  auto success = json_bool(root, "success");
  if (not success) {
    return std::unexpected(NeedleError::MalformedResponse);
  }
  if (not success.value()) {
    return std::unexpected(NeedleError::ModelReportedError);
  }

  NeedleReply reply;
  reply.reasoning = std::string(json_string(root, "reasoning").value_or(""));
  reply.confidence = json_double(root, "confidence").value_or(0.0);

  auto calls = json_array(root, "function_calls");
  if (calls) {
    for (sj::dom::element item : calls.value()) {
      auto name = json_string(item, "name");
      if (not name) {
        return std::unexpected(NeedleError::MalformedResponse);
      }
      ToolCall call;
      call.name = std::string(name.value());

      auto arguments = item.at_key("arguments");
      call.arguments =
          arguments.error() ? std::string{"{}"} : sj::to_string(arguments);
      reply.calls.push_back(std::move(call));
    }
  }
  return reply;
}

std::expected<NeedleReply, NeedleError> NeedleClient::ask(
    std::string_view request,
    std::string_view tools_json,
    const NeedleOptions& options)
{
  if (not loaded()) {
    return std::unexpected(NeedleError::NotLoaded);
  }

  auto configured = configure(tools_json);
  if (not configured.has_value()) {
    return std::unexpected(configured.error());
  }
  needle_reset();

  const std::string input(request);
  const int capacity = static_cast<int>(mBuffer.size());
  const int status = needle_complete(
      input.c_str(),
      nullptr,
      0,
      options.max_tokens,
      mBuffer.data(),
      capacity);

  if (status < 0) {
    return std::unexpected(NeedleError::CompletionFailed);
  }
  // The engine truncates an oversized reply silently, leaving the buffer
  // full, so a reply that fills it may have lost its tail.
  const std::size_t length = ::strnlen(mBuffer.data(), mBuffer.size());
  if (length + 1 >= mBuffer.size()) {
    return std::unexpected(NeedleError::ResponseTruncated);
  }
  return parse_reply(std::string_view(mBuffer.data(), length));
}

}  // namespace cactus
