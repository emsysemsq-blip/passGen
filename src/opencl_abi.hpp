#pragma once
// Minimal declarations for the public OpenCL 1.2 C ABI, based on the Khronos
// OpenCL-Headers specification. No vendor SDK or link-time OpenCL dependency.
// OpenCL and the OpenCL logo are trademarks of Apple Inc. used by permission
// by Khronos. ABI values: registry.khronos.org/OpenCL/sdk/1.2/docs/man/xhtml/.
#include <cstddef>
#include <cstdint>
#ifdef _WIN32
#define PG_CL_CALL __stdcall
#else
#define PG_CL_CALL
#endif
namespace pg { namespace clabi {
using int_t=int32_t;
using uint_t=uint32_t;
using ulong_t=uint64_t;
using bool_t=uint_t;
using bitfield=ulong_t;
using device_type=bitfield;
using context_properties=intptr_t;
struct platform; using platform_id=platform*;
struct device; using device_id=device*;
struct context_; using context=context_*;
struct queue_; using command_queue=queue_*;
struct memory_; using mem=memory_*;
struct program_; using program=program_*;
struct kernel_; using kernel=kernel_*;
struct event_; using event=event_*;
constexpr int_t success=0,device_not_found=-1,platform_not_found=-1001;
constexpr uint_t true_value=1;
constexpr device_type device_cpu=1ULL<<1,device_gpu=1ULL<<2;
constexpr uint_t device_type_info=0x1000,device_max_mem_alloc_size=0x1010,
 device_global_mem_size=0x101f,device_available=0x1027,
 device_compiler_available=0x1028,device_name=0x102b,device_vendor=0x102c,
 driver_version=0x102d,device_host_unified_memory=0x1035;
constexpr context_properties context_platform=0x1084;
constexpr bitfield queue_profiling_enable=1ULL<<1;
constexpr bitfield mem_write_only=1ULL<<1,mem_read_only=1ULL<<2,
 mem_copy_host_ptr=1ULL<<5;
constexpr uint_t program_build_log=0x1183,profiling_command_start=0x1282,
 profiling_command_end=0x1283;
using context_notify=void(PG_CL_CALL*)(const char*,const void*,size_t,void*);
using program_notify=void(PG_CL_CALL*)(program,void*);
} } // namespace pg::clabi
