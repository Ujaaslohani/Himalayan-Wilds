#pragma once
#include <string>
#include <vector>
#include <utility>
#include <memory>

struct Json {
  enum Type { Null, Bool, Number, String, Array, Object };
  Type type = Null;
  bool b = false;
  double n = 0;
  std::string s;
  std::vector<Json> arr;
  std::vector<std::pair<std::string, Json>> obj;

  Json() = default;
  static Json number(double v) { Json j; j.type = Number; j.n = v; return j; }
  static Json boolean(bool v) { Json j; j.type = Bool; j.b = v; return j; }
  static Json string(const std::string& v) { Json j; j.type = String; j.s = v; return j; }
  static Json array() { Json j; j.type = Array; return j; }
  static Json object() { Json j; j.type = Object; return j; }

  Json& set(const std::string& k, const Json& v) {
    for (auto& p : obj) if (p.first == k) { p.second = v; return p.second; }
    obj.emplace_back(k, v);
    return obj.back().second;
  }
  void push(const Json& v) { arr.push_back(v); }
  const Json* get(const std::string& k) const {
    for (auto& p : obj) if (p.first == k) return &p.second;
    return nullptr;
  }
  double num(const std::string& k, double def = 0) const { auto* j = get(k); return j && j->type == Number ? j->n : def; }
  bool flag(const std::string& k, bool def = false) const { auto* j = get(k); return j && j->type == Bool ? j->b : def; }
  std::string str(const std::string& k, const std::string& def = "") const { auto* j = get(k); return j && j->type == String ? j->s : def; }

  std::string dump() const;
  static bool parse(const std::string& text, Json& out);
};
