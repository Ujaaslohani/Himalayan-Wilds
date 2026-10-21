#include "json.h"
#include <cstdio>
#include <cstdlib>
#include <cmath>

static void dumpStr(const std::string& s, std::string& out) {
  out += '"';
  for (unsigned char c : s) {
    if (c == '"') out += "\\\"";
    else if (c == '\\') out += "\\\\";
    else if (c == '\n') out += "\\n";
    else if (c < 0x20) { char buf[8]; std::snprintf(buf, sizeof buf, "\\u%04x", c); out += buf; }
    else out += (char)c;
  }
  out += '"';
}

static void dumpVal(const Json& j, std::string& out) {
  switch (j.type) {
    case Json::Null: out += "null"; break;
    case Json::Bool: out += j.b ? "true" : "false"; break;
    case Json::Number: {
      char buf[64];
      if (std::fabs(j.n - std::round(j.n)) < 1e-9 && std::fabs(j.n) < 1e15) std::snprintf(buf, sizeof buf, "%.0f", j.n);
      else std::snprintf(buf, sizeof buf, "%.6g", j.n);
      out += buf;
      break;
    }
    case Json::String: dumpStr(j.s, out); break;
    case Json::Array:
      out += '[';
      for (size_t i = 0; i < j.arr.size(); i++) { if (i) out += ','; dumpVal(j.arr[i], out); }
      out += ']';
      break;
    case Json::Object:
      out += '{';
      for (size_t i = 0; i < j.obj.size(); i++) {
        if (i) out += ',';
        dumpStr(j.obj[i].first, out);
        out += ':';
        dumpVal(j.obj[i].second, out);
      }
      out += '}';
      break;
  }
}

std::string Json::dump() const {
  std::string out;
  dumpVal(*this, out);
  return out;
}

namespace {
struct Parser {
  const std::string& t;
  size_t i = 0;
  explicit Parser(const std::string& s) : t(s) {}
  void ws() { while (i < t.size() && (t[i] == ' ' || t[i] == '\n' || t[i] == '\r' || t[i] == '\t')) i++; }
  bool lit(const char* s) {
    size_t k = 0;
    while (s[k]) { if (i + k >= t.size() || t[i + k] != s[k]) return false; k++; }
    i += k;
    return true;
  }
  bool str(std::string& out) {
    if (i >= t.size() || t[i] != '"') return false;
    i++;
    while (i < t.size() && t[i] != '"') {
      char c = t[i++];
      if (c == '\\' && i < t.size()) {
        char e = t[i++];
        if (e == 'n') out += '\n';
        else if (e == 't') out += '\t';
        else if (e == 'u' && i + 4 <= t.size()) {
          unsigned v = (unsigned)std::strtoul(t.substr(i, 4).c_str(), nullptr, 16);
          i += 4;
          if (v < 0x80) out += (char)v;
          else if (v < 0x800) { out += (char)(0xC0 | (v >> 6)); out += (char)(0x80 | (v & 63)); }
          else { out += (char)(0xE0 | (v >> 12)); out += (char)(0x80 | ((v >> 6) & 63)); out += (char)(0x80 | (v & 63)); }
        } else out += e;
      } else out += c;
    }
    if (i >= t.size()) return false;
    i++;
    return true;
  }
  bool val(Json& j) {
    ws();
    if (i >= t.size()) return false;
    char c = t[i];
    if (c == '{') {
      i++;
      j.type = Json::Object;
      ws();
      if (i < t.size() && t[i] == '}') { i++; return true; }
      while (true) {
        ws();
        std::string k;
        if (!str(k)) return false;
        ws();
        if (i >= t.size() || t[i] != ':') return false;
        i++;
        Json v;
        if (!val(v)) return false;
        j.obj.emplace_back(k, std::move(v));
        ws();
        if (i < t.size() && t[i] == ',') { i++; continue; }
        if (i < t.size() && t[i] == '}') { i++; return true; }
        return false;
      }
    }
    if (c == '[') {
      i++;
      j.type = Json::Array;
      ws();
      if (i < t.size() && t[i] == ']') { i++; return true; }
      while (true) {
        Json v;
        if (!val(v)) return false;
        j.arr.push_back(std::move(v));
        ws();
        if (i < t.size() && t[i] == ',') { i++; continue; }
        if (i < t.size() && t[i] == ']') { i++; return true; }
        return false;
      }
    }
    if (c == '"') { j.type = Json::String; return str(j.s); }
    if (lit("true")) { j.type = Json::Bool; j.b = true; return true; }
    if (lit("false")) { j.type = Json::Bool; j.b = false; return true; }
    if (lit("null")) { j.type = Json::Null; return true; }
    const char* start = t.c_str() + i;
    char* end = nullptr;
    double v = std::strtod(start, &end);
    if (end == start) return false;
    i += (size_t)(end - start);
    j.type = Json::Number;
    j.n = v;
    return true;
  }
};
}  // namespace

bool Json::parse(const std::string& text, Json& out) {
  Parser p(text);
  out = Json();
  return p.val(out);
}
