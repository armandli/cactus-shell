#include <json_util.h>

#include <cassert>

#include <string>
#include <string_view>
#include <utility>

namespace cactus {

namespace sj = simdjson;

std::string_view describe(JsonError error) {
  switch (error) {
    case JsonError::ParseFailed: return "malformed JSON";

    break; case JsonError::KeyNotFound: return "key not found";

    break; case JsonError::TypeMismatch: return "value has unexpected type";

    break; case JsonError::Unbalanced: return "unclosed object or array";

    break; default: assert(false); // should never get here
  }
  return "unknown error";
}

void JsonBuilder::punctuate() {
  if (pending_comma_) {
    builder_.append_comma();
    pending_comma_ = false;
  }
}

JsonBuilder& JsonBuilder::begin_object() {
  punctuate();
  builder_.start_object();
  depth_ += 1;
  return *this;
}

JsonBuilder& JsonBuilder::end_object() {
  builder_.end_object();
  depth_ -= 1;
  pending_comma_ = true;
  return *this;
}

JsonBuilder& JsonBuilder::begin_array() {
  punctuate();
  builder_.start_array();
  depth_ += 1;
  return *this;
}

JsonBuilder& JsonBuilder::end_array() {
  builder_.end_array();
  depth_ -= 1;
  pending_comma_ = true;
  return *this;
}

JsonBuilder& JsonBuilder::key(std::string_view name) {
  punctuate();
  builder_.escape_and_append_with_quotes(name);
  builder_.append_colon();
  return *this;
}

JsonBuilder& JsonBuilder::value(std::string_view text) {
  punctuate();
  builder_.escape_and_append_with_quotes(text);
  pending_comma_ = true;
  return *this;
}

JsonBuilder& JsonBuilder::value(bool flag) {
  punctuate();
  builder_.append_raw(flag ? "true" : "false");
  pending_comma_ = true;
  return *this;
}

JsonBuilder& JsonBuilder::value(std::int64_t number) {
  punctuate();
  builder_.append_raw(std::to_string(number));
  pending_comma_ = true;
  return *this;
}

JsonBuilder& JsonBuilder::value(double number) {
  punctuate();
  builder_.append(number);
  pending_comma_ = true;
  return *this;
}

JsonBuilder& JsonBuilder::null_value() {
  punctuate();
  builder_.append_null();
  pending_comma_ = true;
  return *this;
}

JsonBuilder& JsonBuilder::raw_value(std::string_view json) {
  punctuate();
  builder_.append_raw(json);
  pending_comma_ = true;
  return *this;
}

JsonBuilder& JsonBuilder::field(std::string_view name, std::string_view text) {
  return key(name).value(text);
}

JsonBuilder& JsonBuilder::field(std::string_view name, bool flag) {
  return key(name).value(flag);
}

JsonBuilder& JsonBuilder::field(std::string_view name, std::int64_t number) {
  return key(name).value(number);
}

JsonBuilder& JsonBuilder::field(std::string_view name, double number) {
  return key(name).value(number);
}

std::expected<std::string, JsonError> JsonBuilder::str() const {
  if (not balanced()) {
    return std::unexpected(JsonError::Unbalanced);
  }
  std::string_view rendered;
  if (builder_.view().get(rendered)) {
    return std::unexpected(JsonError::ParseFailed);
  }
  return std::string(rendered);
}

void JsonBuilder::clear() {
  builder_.clear();
  depth_ = 0;
  pending_comma_ = false;
}

std::expected<JsonDoc, JsonError> JsonDoc::parse(std::string_view text) {
  JsonDoc doc;
  doc.parser_ = std::make_unique<sj::dom::parser>();
  doc.source_ = std::make_unique<sj::padded_string>(text);

  auto parsed = doc.parser_->parse(*doc.source_);
  if (parsed.error()) {
    return std::unexpected(JsonError::ParseFailed);
  }
  doc.root_ = parsed.value();
  return doc;
}

namespace {

std::expected<sj::dom::element, JsonError> lookup(
    sj::dom::element parent,
    std::string_view key)
{
  auto found = parent.at_key(key);
  if (found.error() == sj::NO_SUCH_FIELD) {
    return std::unexpected(JsonError::KeyNotFound);
  }
  if (found.error()) {
    return std::unexpected(JsonError::TypeMismatch);
  }
  return found.value();
}

template<typename T>
std::expected<T, JsonError> lookup_as(
    sj::dom::element parent,
    std::string_view key)
{
  auto found = lookup(parent, key);
  if (not found) {
    return std::unexpected(found.error());
  }
  T out{};
  if (found.value().get(out)) {
    return std::unexpected(JsonError::TypeMismatch);
  }
  return out;
}

}  // namespace

std::expected<std::string_view, JsonError> json_string(
    sj::dom::element parent,
    std::string_view key)
{
  return lookup_as<std::string_view>(parent, key);
}

std::expected<std::int64_t, JsonError> json_int(
    sj::dom::element parent,
    std::string_view key)
{
  return lookup_as<std::int64_t>(parent, key);
}

std::expected<double, JsonError> json_double(
    sj::dom::element parent,
    std::string_view key)
{
  return lookup_as<double>(parent, key);
}

std::expected<bool, JsonError> json_bool(
    sj::dom::element parent,
    std::string_view key)
{
  return lookup_as<bool>(parent, key);
}

std::expected<sj::dom::array, JsonError> json_array(
    sj::dom::element parent,
    std::string_view key)
{
  return lookup_as<sj::dom::array>(parent, key);
}

std::expected<sj::dom::object, JsonError> json_object(
    sj::dom::element parent,
    std::string_view key)
{
  return lookup_as<sj::dom::object>(parent, key);
}

}  // namespace cactus
