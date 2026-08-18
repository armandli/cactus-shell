#include <needle.h>

#include <cassert>

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <json_util.h>

namespace cactus {

namespace sj = simdjson;

std::string_view describe(NeedleError error) {
  switch (error) {
    case NeedleError::ModelLoadFailed: return "could not load model";

    break; case NeedleError::NotLoaded: return "no model loaded";

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
  : system_prompt_(std::move(system_prompt)) {
}

NeedleClient::~NeedleClient() {
  unload();
}

NeedleClient::NeedleClient(NeedleClient&& other) noexcept
  : system_prompt_(std::move(other.system_prompt_)),
    model_(other.model_),
    buffer_(std::move(other.buffer_)) {
  other.model_ = nullptr;
}

NeedleClient& NeedleClient::operator=(NeedleClient&& other) noexcept {
  if (this != &other) {
    unload();
    system_prompt_ = std::move(other.system_prompt_);
    model_ = other.model_;
    buffer_ = std::move(other.buffer_);
    other.model_ = nullptr;
  }
  return *this;
}

std::expected<void, NeedleError> NeedleClient::load(
    const std::string& model_path)
{
  unload();
  model_ = cactus_init(model_path.c_str(), nullptr, false);
  if (model_ == nullptr) {
    return std::unexpected(NeedleError::ModelLoadFailed);
  }
  return {};
}

void NeedleClient::unload() {
  if (model_ != nullptr) {
    cactus_destroy(model_);
    model_ = nullptr;
  }
}

void NeedleClient::reset() {
  if (model_ != nullptr) {
    cactus_reset(model_);
  }
}

std::string NeedleClient::render_messages(std::string_view request) const {
  JsonBuilder builder;
  builder.begin_array();
  if (not system_prompt_.empty()) {
    builder.begin_object()
        .field("role", std::string_view{"system"})
        .field("content", std::string_view{system_prompt_})
        .end_object();
  }
  builder.begin_object()
      .field("role", std::string_view{"user"})
      .field("content", request)
      .end_object();
  builder.end_array();
  return builder.str().value_or(std::string{"[]"});
}

std::string NeedleClient::render_options(const NeedleOptions& options) {
  JsonBuilder builder;
  builder.begin_object()
      .field("max_tokens", options.max_tokens)
      .field("temperature", options.temperature)
      .field("top_p", options.top_p)
      .field("top_k", options.top_k)
      .field("force_tools", options.force_tools)
      .field("auto_handoff", options.auto_handoff)
      .end_object();
  return builder.str().value_or(std::string{"{}"});
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
  reply.text = std::string(json_string(root, "response").value_or(""));
  reply.confidence = json_double(root, "confidence").value_or(0.0);
  reply.cloud_handoff = json_bool(root, "cloud_handoff").value_or(false);

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

  const std::string messages = render_messages(request);
  const std::string rendered_options = render_options(options);
  const std::string tools(tools_json);

  const int written = cactus_complete(
      model_,
      messages.c_str(),
      buffer_.data(),
      buffer_.size(),
      rendered_options.c_str(),
      tools.empty() ? nullptr : tools.c_str(),
      nullptr,
      nullptr,
      nullptr,
      0);

  if (written < 0) {
    return std::unexpected(NeedleError::CompletionFailed);
  }
  if (static_cast<std::size_t>(written) >= buffer_.size()) {
    return std::unexpected(NeedleError::ResponseTruncated);
  }
  return parse_reply(std::string_view(buffer_.data()));
}

}  // namespace cactus
