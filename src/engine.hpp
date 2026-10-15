#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <atomic>
#include <functional>
namespace pg {
struct Config {
 std::vector<std::string> tokens;
 unsigned min_length=1, max_length=1, threads=1;
 bool lower=false, upper=false, digit=false, symbol=false, no_repeat=false;
 std::string prefix, suffix;
};
struct Progress {
 std::atomic<uint64_t> scanned{0}, written{0}, bytes{0}, total{0};
 std::atomic<bool> stop{false};
};
std::vector<std::string> tokenize(const std::string&, bool words);
uint64_t total(const Config&);
std::string candidate(const Config&, uint64_t);
bool accepts(const Config&, const std::string&);
// output must not exist for a new job; resume loads all generation settings from checkpoint.
void run(Config config, const std::string& output, bool resume, Progress&,
         const std::function<void(const Config&)>& loaded = {});
}
