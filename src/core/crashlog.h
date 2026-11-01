#pragma once
#include <string>
// Writes crash_<time>.txt (version, error, call addresses, last game messages) into the save folder.
namespace crashlog {
void init(const std::string& dir);
void note(const std::string& line);              // remember a recent event (ring buffer)
void setState(const std::string& state);         // short game state line, refreshed now and then
}
