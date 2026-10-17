#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace pg {
// ASCII class bits: lower=1, upper=2, digit=4, punctuation=8.
struct GpuToken { std::string bytes; unsigned ascii_mask=0; };
struct GpuDevice {
 std::string name,vendor,driver;
 uint64_t global_memory=0,max_allocation=0;
 bool unified_memory=false,available=false,gpu=true;
};
struct GpuInventory { std::vector<GpuDevice> devices; std::string status; };
// CPU OpenCL devices are permitted only by explicitly opted-in test callers.
GpuInventory query_gpu(bool allow_cpu_for_tests=false);
struct GpuResult {
 std::string text;
 uint64_t lines=0;
 size_t span=0;
 double device_seconds=0;
};
// An optional, dynamically loaded OpenCL 1.2 engine. No driver is installed or
// downloaded. A missing/incompatible driver leaves available()==false.
// The driver module is cached for the process; restart after a driver change.
// One instance is owned and called by one generation thread.
class GpuEngine {
 struct Impl;
 std::unique_ptr<Impl> impl;
public:
 GpuEngine(std::vector<GpuToken> tokens,size_t max_host_bytes,
           bool allow_cpu_for_tests=false);
 ~GpuEngine();
 GpuEngine(const GpuEngine&)=delete;
 GpuEngine& operator=(const GpuEngine&)=delete;
 bool available() const;
 std::string name() const;
 std::string status() const;
 // Includes raw rows, flags and the simultaneous compacted host result;
 // shared-memory devices also charge device rows/flags against this budget.
 // Zero means this row does not fit. The result never exceeds 65,536.
 size_t max_span(size_t row_bytes) const;
 // Digits are little-endian, one base-tokens.size() digit per token position.
 // The caller must keep the entire span inside the given token-length range.
 // A completed call always returns the entire span in rank order. A stop
 // before launch returns span==0; a stop after launch retains completed rows.
 GpuResult generate(const std::vector<uint32_t>& digits,unsigned token_length,
                    size_t span,unsigned required_mask,bool no_repeat,
                    const std::string& prefix,const std::string& suffix,
                    const std::atomic<bool>* stop=nullptr,unsigned target_percent=70);
};
} // namespace pg
