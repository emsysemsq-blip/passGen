#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <atomic>
#include <functional>
#include "index.hpp"
#include "resources.hpp"
namespace pg {
struct Config {
 std::vector<std::string> tokens;
 unsigned min_length=1, max_length=1, threads=1;
 bool character_lengths=false;
 bool lower=false, upper=false, digit=false, symbol=false, no_repeat=false;
 unsigned resource_percent=0; // 0: manual/unrestricted; GUI and CLI default to 70.
 std::string prefix, suffix;
};
struct Progress {
 SharedIndex scanned, total;
 std::atomic<uint64_t> written{0}, bytes{0};
 std::atomic<bool> stop{false};
 std::atomic<bool> disk_limit{false};
 std::atomic<unsigned> active_workers{0};
 std::atomic<uint64_t> buffer_bytes{0}, disk_budget_bytes{0};
};
std::vector<std::string> tokenize(const std::string&, bool words);
void add_symbols(Config&, const std::string&);
void add_emojis(Config&, const std::string&);
void add_persian(Config&);
void add_shift_letters(Config&);
size_t characters(const std::string&);
unsigned default_max_length(const Config&);
Index total(const Config&);
std::string candidate(const Config&, Index);
bool accepts(const Config&, const std::string&);
struct Preview { std::vector<std::string> samples; size_t inspected=0; };
Preview preview(const Config&, size_t maximum=12);
// output must not exist for a new job; resume loads all generation settings from checkpoint.
void run(Config config, const std::string& output, bool resume, Progress&,
         const std::function<void(const Config&)>& loaded = {},
         const ResourceSnapshot* resource_snapshot = nullptr);
}
