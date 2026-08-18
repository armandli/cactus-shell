#ifndef JSON_UTIL_H
#define JSON_UTIL_H

#include <cstdint>

#include <expected>
#include <memory>
#include <string>
#include <string_view>

#include <simdjson.h>
#include <simdjson/builder.h>

namespace cactus {

enum class JsonError : int {
  ParseFailed = 0,
  KeyNotFound,
  TypeMismatch,
  Unbalanced,
};

std::string_view describe(JsonError error);

// Streaming JSON writer over simdjson's string_builder. Separators are emitted
// automatically, so callers never write commas or colons themselves.
struct JsonBuilder {
  JsonBuilder() = default;

  JsonBuilder& begin_object();
  JsonBuilder& end_object();
  JsonBuilder& begin_array();
  JsonBuilder& end_array();

  JsonBuilder& key(std::string_view name);

  JsonBuilder& value(std::string_view text);
  JsonBuilder& value(bool flag);
  JsonBuilder& value(std::int64_t number);
  JsonBuilder& value(double number);
  JsonBuilder& null_value();
  JsonBuilder& raw_value(std::string_view json);

  JsonBuilder& field(std::string_view name, std::string_view text);
  JsonBuilder& field(std::string_view name, bool flag);
  JsonBuilder& field(std::string_view name, std::int64_t number);
  JsonBuilder& field(std::string_view name, double number);

  bool balanced() const { return depth_ == 0; }

  std::expected<std::string, JsonError> str() const;
  void clear();

protected:
  void punctuate();

  simdjson::builder::string_builder builder_;
  int depth_ = 0;
  bool pending_comma_ = false;
};

// Owns the parser and the padded copy of the source text that the returned
// element points into, so the document stays valid as long as JsonDoc lives.
struct JsonDoc {
  JsonDoc() = default;
  JsonDoc(JsonDoc&&) noexcept = default;
  JsonDoc& operator=(JsonDoc&&) noexcept = default;
  JsonDoc(const JsonDoc&) = delete;
  JsonDoc& operator=(const JsonDoc&) = delete;

  simdjson::dom::element root() const { return root_; }

  static std::expected<JsonDoc, JsonError> parse(std::string_view text);

protected:
  std::unique_ptr<simdjson::dom::parser> parser_;
  std::unique_ptr<simdjson::padded_string> source_;
  simdjson::dom::element root_;
};

std::expected<std::string_view, JsonError> json_string(
    simdjson::dom::element parent,
    std::string_view key);

std::expected<std::int64_t, JsonError> json_int(
    simdjson::dom::element parent,
    std::string_view key);

std::expected<double, JsonError> json_double(
    simdjson::dom::element parent,
    std::string_view key);

std::expected<bool, JsonError> json_bool(
    simdjson::dom::element parent,
    std::string_view key);

std::expected<simdjson::dom::array, JsonError> json_array(
    simdjson::dom::element parent,
    std::string_view key);

std::expected<simdjson::dom::object, JsonError> json_object(
    simdjson::dom::element parent,
    std::string_view key);

}  // namespace cactus

#endif
