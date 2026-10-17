#include "gpu.hpp"
#include "opencl_abi.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <thread>
#include <utility>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace pg {
namespace {
using namespace clabi;
constexpr size_t hard_span=65536,initial_span=4096,maximum_rows_bytes=16*1024*1024;
unsigned classes(const std::string& text) {
 unsigned mask=0;
 for(unsigned char ch:text) {
  if(ch>='a'&&ch<='z')mask|=1;
  else if(ch>='A'&&ch<='Z')mask|=2;
  else if(ch>='0'&&ch<='9')mask|=4;
  else if(ch>=33&&ch<=126)mask|=8;
 }
 return mask;
}
void check(int_t code,const char* operation) {
 if(code!=success)throw std::runtime_error(std::string(operation)+" (OpenCL "+std::to_string(code)+")");
}
class Api {
#ifdef _WIN32
 HMODULE library=nullptr;
#else
 void* library=nullptr;
#endif
 template<class T> void load(T& function,const char* name) {
#ifdef _WIN32
  auto address=GetProcAddress(library,name);
#else
  auto address=dlsym(library,name);
#endif
  if(!address)throw std::runtime_error(std::string("OpenCL runtime is missing ")+name);
  static_assert(sizeof(function)==sizeof(address),"OpenCL function pointer size");
  std::memcpy(&function,&address,sizeof(function));
 }
public:
#define PG_CL_FUNCTION(result,name,...) result(PG_CL_CALL* name)(__VA_ARGS__)=nullptr
 PG_CL_FUNCTION(int_t,GetPlatformIDs,uint_t,platform_id*,uint_t*);
 PG_CL_FUNCTION(int_t,GetDeviceIDs,platform_id,device_type,uint_t,device_id*,uint_t*);
 PG_CL_FUNCTION(int_t,GetDeviceInfo,device_id,uint_t,size_t,void*,size_t*);
 PG_CL_FUNCTION(context,CreateContext,const context_properties*,uint_t,const device_id*,context_notify,void*,int_t*);
 PG_CL_FUNCTION(int_t,ReleaseContext,context);
 PG_CL_FUNCTION(command_queue,CreateCommandQueue,context,device_id,bitfield,int_t*);
 PG_CL_FUNCTION(int_t,ReleaseCommandQueue,command_queue);
 PG_CL_FUNCTION(program,CreateProgramWithSource,context,uint_t,const char**,const size_t*,int_t*);
 PG_CL_FUNCTION(int_t,BuildProgram,program,uint_t,const device_id*,const char*,program_notify,void*);
 PG_CL_FUNCTION(int_t,GetProgramBuildInfo,program,device_id,uint_t,size_t,void*,size_t*);
 PG_CL_FUNCTION(int_t,ReleaseProgram,program);
 PG_CL_FUNCTION(kernel,CreateKernel,program,const char*,int_t*);
 PG_CL_FUNCTION(int_t,ReleaseKernel,kernel);
 PG_CL_FUNCTION(mem,CreateBuffer,context,bitfield,size_t,void*,int_t*);
 PG_CL_FUNCTION(int_t,ReleaseMemObject,mem);
 PG_CL_FUNCTION(int_t,SetKernelArg,kernel,uint_t,size_t,const void*);
 PG_CL_FUNCTION(int_t,EnqueueWriteBuffer,command_queue,mem,bool_t,size_t,size_t,const void*,uint_t,const event*,event*);
 PG_CL_FUNCTION(int_t,EnqueueReadBuffer,command_queue,mem,bool_t,size_t,size_t,void*,uint_t,const event*,event*);
 PG_CL_FUNCTION(int_t,EnqueueNDRangeKernel,command_queue,kernel,uint_t,const size_t*,const size_t*,const size_t*,uint_t,const event*,event*);
 PG_CL_FUNCTION(int_t,Finish,command_queue);
 PG_CL_FUNCTION(int_t,GetEventProfilingInfo,event,uint_t,size_t,void*,size_t*);
 PG_CL_FUNCTION(int_t,ReleaseEvent,event);
#undef PG_CL_FUNCTION
 Api() {
  // ICD loaders keep process-wide platform/vendor tables, and some drivers
  // also own background threads. Repeated dlclose/FreeLibrary cannot release
  // those tables through the OpenCL API and can strand them on every probe.
  // Retain one module for the process, while every job's OpenCL objects below
  // remain owned and released normally. Function-local initialization is
  // thread-safe. Installing/changing a driver requires restarting PassGen.
#ifdef _WIN32
  static const HMODULE process_module=[]() -> HMODULE {
   // Absolute System32 path excludes the download/current directory.
   wchar_t system[MAX_PATH+1]{};
   const UINT length=GetSystemDirectoryW(system,MAX_PATH);
   if(!length||length>=MAX_PATH)return nullptr;
   const std::wstring path=std::wstring(system,length)+L"\\OpenCL.dll";
   return LoadLibraryExW(path.c_str(),nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
  }();
#else
  static void* const process_module=dlopen("libOpenCL.so.1",RTLD_NOW|RTLD_LOCAL);
#endif
  library=process_module;
  if(!library)throw std::runtime_error("OpenCL runtime is not installed; CPU remains available");
  try {
#define PG_CL_LOAD(name) load(name,"cl" #name)
   PG_CL_LOAD(GetPlatformIDs);PG_CL_LOAD(GetDeviceIDs);PG_CL_LOAD(GetDeviceInfo);
   PG_CL_LOAD(CreateContext);PG_CL_LOAD(ReleaseContext);PG_CL_LOAD(CreateCommandQueue);
   PG_CL_LOAD(ReleaseCommandQueue);PG_CL_LOAD(CreateProgramWithSource);PG_CL_LOAD(BuildProgram);
   PG_CL_LOAD(GetProgramBuildInfo);PG_CL_LOAD(ReleaseProgram);PG_CL_LOAD(CreateKernel);
   PG_CL_LOAD(ReleaseKernel);PG_CL_LOAD(CreateBuffer);PG_CL_LOAD(ReleaseMemObject);
   PG_CL_LOAD(SetKernelArg);PG_CL_LOAD(EnqueueWriteBuffer);PG_CL_LOAD(EnqueueReadBuffer);
   PG_CL_LOAD(EnqueueNDRangeKernel);PG_CL_LOAD(Finish);PG_CL_LOAD(GetEventProfilingInfo);PG_CL_LOAD(ReleaseEvent);
#undef PG_CL_LOAD
  }catch(...) { close();throw; }
 }
 ~Api(){close();}
 Api(const Api&)=delete;
 Api& operator=(const Api&)=delete;
 void close() {
  // Borrowed from process_module; only detach this function table.
  library=nullptr;
 }
};
template<class T> T info(Api& api,device_id device,uint_t key,T fallback={}) {
 T result=fallback;
 if(api.GetDeviceInfo(device,key,sizeof(result),&result,nullptr)!=success)return fallback;
 return result;
}
std::string text_info(Api& api,device_id device,uint_t key) {
 size_t bytes=0;
 if(api.GetDeviceInfo(device,key,0,nullptr,&bytes)!=success||!bytes||bytes>65536)return {};
 std::string result(bytes,'\0');
 if(api.GetDeviceInfo(device,key,bytes,&result[0],nullptr)!=success)return {};
 while(!result.empty()&&result.back()=='\0')result.pop_back();
 return result;
}
struct DeviceRef { platform_id platform=nullptr;device_id device=nullptr;GpuDevice info; };
std::vector<DeviceRef> devices(Api& api,bool allow_cpu) {
 uint_t platforms_count=0;
 const auto code=api.GetPlatformIDs(0,nullptr,&platforms_count);
 if(code==platform_not_found)return {};
 check(code,"Cannot enumerate OpenCL platforms");
 if(!platforms_count)return {};
 if(platforms_count>1024)throw std::runtime_error("Invalid OpenCL platform count");
 std::vector<platform_id> platforms(platforms_count);
 check(api.GetPlatformIDs(platforms_count,platforms.data(),nullptr),"Cannot read OpenCL platforms");
 std::vector<DeviceRef> result;
 std::string failures;
 for(auto platform:platforms) {
  uint_t count=0;
  const device_type filter=device_gpu|(allow_cpu?device_cpu:0);
  const auto error=api.GetDeviceIDs(platform,filter,0,nullptr,&count);
  if(error==device_not_found)continue;
  if(error!=success){failures+="Device enumeration error "+std::to_string(error)+"; ";continue;}
  if(!count)continue;
  if(count>4096){failures+="Invalid OpenCL device count; ";continue;}
  std::vector<device_id> ids(count);
  const auto read_error=api.GetDeviceIDs(platform,filter,count,ids.data(),nullptr);
  if(read_error!=success){failures+="Device read error "+std::to_string(read_error)+"; ";continue;}
  for(auto device:ids) {
   DeviceRef entry;entry.platform=platform;entry.device=device;
   auto& value=entry.info;
   value.gpu=(info<device_type>(api,device,device_type_info)&device_gpu)!=0;
   if(!value.gpu&&!allow_cpu)continue; // Enforce GPU-only even if a driver ignores its filter.
   value.name=text_info(api,device,device_name);value.vendor=text_info(api,device,device_vendor);
   value.driver=text_info(api,device,driver_version);
   value.global_memory=info<ulong_t>(api,device,device_global_mem_size);
   value.max_allocation=info<ulong_t>(api,device,device_max_mem_alloc_size);
   value.unified_memory=info<bool_t>(api,device,device_host_unified_memory)!=0;
   value.available=info<bool_t>(api,device,device_available)!=0&&
                   info<bool_t>(api,device,device_compiler_available)!=0;
   result.push_back(std::move(entry));
  }
 }
 if(result.empty()&&!failures.empty())throw std::runtime_error(failures);
 std::stable_sort(result.begin(),result.end(),[](const DeviceRef& a,const DeviceRef& b) {
  if(a.info.gpu!=b.info.gpu)return a.info.gpu;
  if(a.info.available!=b.info.available)return a.info.available;
  if(a.info.unified_memory!=b.info.unified_memory)return !a.info.unified_memory;
  return a.info.global_memory>b.info.global_memory;
 });
 return result;
}
const char* source=R"CLC(
__kernel void passgen(__global const uchar* tokens,__global const uint* masks,
 __global const uint* base_digits,uint length,uint width,uint count,uint span,
 uint required,uint no_repeat,uint affix_mask,
 __global const uchar* prefix,uint prefix_bytes,
 __global const uchar* suffix,uint suffix_bytes,uint row_bytes,
 __global uchar* rows,__global uchar* flags) {
 uint gid=(uint)get_global_id(0);
 if(gid>=span)return;
 uint carry=gid;uint mask=affix_mask;uint valid=1;uint previous=0xffffffffu;
 size_t row=(size_t)gid*row_bytes;
 for(uint i=0;i<prefix_bytes;i++)rows[row+i]=prefix[i];
 for(uint i=0;i<length;i++) {
  uint sum=base_digits[i]+carry;
  uint digit=sum%count;carry=sum/count;
  mask|=masks[digit];
  if(no_repeat&&digit==previous)valid=0;
  previous=digit;
  size_t offset=row+prefix_bytes+(length-1-i)*width;
  uint token=digit*width;
  for(uint j=0;j<width;j++)rows[offset+j]=tokens[token+j];
 }
 valid=valid&&!carry&&((mask&required)==required);
 flags[gid]=(uchar)valid;
 size_t offset=row+prefix_bytes+length*width;
 for(uint i=0;i<suffix_bytes;i++)rows[offset++]=suffix[i];
 rows[offset]=(uchar)10;
}
)CLC";
} // namespace

GpuInventory query_gpu(bool allow_cpu_for_tests) {
 GpuInventory result;
 try {
  Api api;
  for(auto& entry:devices(api,allow_cpu_for_tests))result.devices.push_back(std::move(entry.info));
  result.status=result.devices.empty()?"No usable OpenCL GPU was detected; CPU remains available":
                "OpenCL devices detected";
 }catch(const std::exception& error){result.status=error.what();}
 return result;
}

struct GpuEngine::Impl {
 std::unique_ptr<Api> api;
 std::vector<GpuToken> tokens;
 std::vector<uint_t> masks;
 std::vector<unsigned char> table;
 size_t host_budget=0,width=0,dynamic_span=initial_span;
 uint64_t device_budget=0,max_allocation=0;
 std::string device_name,status="OpenCL GPU is unavailable";
 context ctx=nullptr;command_queue queue=nullptr;program prog=nullptr;kernel function=nullptr;
 mem table_buffer=nullptr,masks_buffer=nullptr,digits_buffer=nullptr;
 mem prefix_buffer=nullptr,suffix_buffer=nullptr,rows_buffer=nullptr,flags_buffer=nullptr;
 std::string cached_prefix,cached_suffix;
 size_t rows_capacity=0,flags_capacity=0;
 bool verified=false,unified_memory=false;
 std::chrono::steady_clock::time_point next_launch{};
 explicit Impl(std::vector<GpuToken> input,size_t budget):tokens(std::move(input)),host_budget(budget){}
 ~Impl(){release();}
 void release_mem(mem& value){if(value&&api)api->ReleaseMemObject(value);value=nullptr;}
 void release() {
  // A failing transfer can leave work queued. Drain it before releasing job
  // resources; release-queue alone permits deferred destruction.
  if(queue&&api)api->Finish(queue);
  release_mem(table_buffer);release_mem(masks_buffer);release_mem(digits_buffer);
  release_mem(prefix_buffer);release_mem(suffix_buffer);release_mem(rows_buffer);release_mem(flags_buffer);
  if(api) {
   if(function)api->ReleaseKernel(function);
   if(prog)api->ReleaseProgram(prog);
   if(queue)api->ReleaseCommandQueue(queue);
   if(ctx)api->ReleaseContext(ctx);
  }
  function=nullptr;prog=nullptr;queue=nullptr;ctx=nullptr;
  rows_capacity=flags_capacity=0;
 }
 mem buffer(bitfield flags,size_t bytes,const void* data=nullptr) {
  int_t error=0;
  auto result=api->CreateBuffer(ctx,flags|(data?mem_copy_host_ptr:0),bytes,
                                const_cast<void*>(data),&error);
  check(error,"Cannot allocate bounded OpenCL buffer");
  if(!result)throw std::runtime_error("OpenCL returned an empty buffer");
  return result;
 }
 void initialize(const DeviceRef& ref) {
  device_name=ref.info.name;device_budget=ref.info.global_memory/100*70;
  unified_memory=ref.info.unified_memory;
  max_allocation=ref.info.max_allocation;
  if(device_budget<256*1024||max_allocation<64*1024)
   throw std::runtime_error("GPU memory budget is too small");
  int_t error=0;
  context_properties properties[]={context_platform,reinterpret_cast<context_properties>(ref.platform),0};
  ctx=api->CreateContext(properties,1,&ref.device,nullptr,nullptr,&error);
  check(error,"Cannot create OpenCL context");
  queue=api->CreateCommandQueue(ctx,ref.device,queue_profiling_enable,&error);
  check(error,"Cannot create OpenCL profiling queue");
  const size_t bytes=std::strlen(source);
  prog=api->CreateProgramWithSource(ctx,1,&source,&bytes,&error);
  check(error,"Cannot create OpenCL program");
  error=api->BuildProgram(prog,1,&ref.device,"-cl-std=CL1.2",nullptr,nullptr);
  if(error!=success) {
   size_t size=0;std::string log;
   if(api->GetProgramBuildInfo(prog,ref.device,program_build_log,0,nullptr,&size)==success&&size&&size<=1024*1024) {
    log.resize(size);
    if(api->GetProgramBuildInfo(prog,ref.device,program_build_log,size,&log[0],nullptr)!=success)log.clear();
    log.resize((std::min)(log.size(),size_t(2048)));
    while(!log.empty()&&log.back()=='\0')log.pop_back();
   }
   throw std::runtime_error("OpenCL kernel build failed ("+std::to_string(error)+"): "+log);
  }
  function=api->CreateKernel(prog,"passgen",&error);check(error,"Cannot create OpenCL kernel");
  table_buffer=buffer(mem_read_only,table.size(),table.data());
  masks_buffer=buffer(mem_read_only,masks.size()*sizeof(uint_t),masks.data());
  digits_buffer=buffer(mem_read_only,128*sizeof(uint_t));
  static const unsigned char empty=0;
  prefix_buffer=buffer(mem_read_only,1,&empty);suffix_buffer=buffer(mem_read_only,1,&empty);
  status="OpenCL GPU ready: "+device_name;
  if(!ref.info.gpu)status="OpenCL CPU TEST DEVICE (not GPU acceleration): "+device_name;
 }
 template<class T> void arg(uint_t index,const T& value) {
  check(api->SetKernelArg(function,index,sizeof(T),&value),"Cannot set OpenCL argument");
 }
 size_t capacity(size_t row_bytes) const {
  if(!function||!row_bytes||row_bytes>65536)return 0;
  // Reserve room for the alphabet, affixes, digit seed and buffer metadata.
  const uint64_t reserve=table.size()+masks.size()*sizeof(uint_t)+128*sizeof(uint_t)+131072;
  if(device_budget<=reserve||host_budget<=reserve)return 0;
  const uint64_t gpu_rows=(std::min)({uint64_t(maximum_rows_bytes),max_allocation,device_budget-reserve});
  uint64_t count=(std::min)({uint64_t(hard_span),uint64_t(dynamic_span),
   gpu_rows/row_bytes,(device_budget-reserve)/(row_bytes+1),
   uint64_t(host_budget-reserve)/((unified_memory?3:2)*row_bytes+(unified_memory?2:1)),max_allocation});
  return static_cast<size_t>(count);
 }
 void set_affix(mem& target,std::string& cached,const std::string& value) {
  if(value==cached)return;
  release_mem(target);
  static const unsigned char empty=0;
  const void* data=value.empty()?static_cast<const void*>(&empty):static_cast<const void*>(value.data());
  target=buffer(mem_read_only,(std::max)(size_t(1),value.size()),data);
  cached=value;
 }
 void verify(const std::vector<uint32_t>& seed,unsigned length,size_t span,
             unsigned required,bool no_repeat,const std::string& prefix,
             const std::string& suffix,size_t row_bytes,
             const std::vector<unsigned char>& rows,const std::vector<unsigned char>& flags) {
  std::vector<uint32_t> digits=seed;
  for(size_t row=0;row<span;++row) {
   unsigned mask=classes(prefix)|classes(suffix);bool valid=true;
   std::string expected=prefix;
   for(unsigned n=length;n>0;--n) {
    const auto token=digits[n-1];mask|=masks[token];expected+=tokens[token].bytes;
    if(no_repeat&&n<length&&digits[n]==token)valid=false;
   }
   valid=valid&&((mask&required)==required);expected+=suffix;expected+='\n';
   if(flags[row]!=(valid?1:0)||
      (valid&&std::memcmp(rows.data()+row*row_bytes,expected.data(),row_bytes)!=0))
    throw std::runtime_error("OpenCL verification failed; discard this batch and use CPU");
   for(unsigned i=0;i<length;++i){if(++digits[i]<tokens.size())break;digits[i]=0;}
  }
  verified=true;
 }
};

GpuEngine::GpuEngine(std::vector<GpuToken> tokens,size_t max_host_bytes,bool allow_cpu_for_tests)
 :impl(new Impl(std::move(tokens),max_host_bytes)) {
 auto& p=*impl;
 if(p.tokens.empty()||p.tokens.size()>4096) {p.status="GPU supports 1..4096 fixed-width tokens";return;}
 p.width=p.tokens.front().bytes.size();
 if(!p.width||p.width>128) {p.status="GPU token byte width must be 1..128";return;}
 std::vector<std::string> sorted;
 for(auto& token:p.tokens) {
  if(token.bytes.size()!=p.width) {p.status="Variable-width tokens use CPU";return;}
  if(token.bytes.find_first_of("\r\n")!=std::string::npos||token.bytes.find('\0')!=std::string::npos) {
   p.status="GPU token contains a line separator or NUL";return;
  }
  token.ascii_mask=classes(token.bytes);p.masks.push_back(token.ascii_mask);
  p.table.insert(p.table.end(),token.bytes.begin(),token.bytes.end());sorted.push_back(token.bytes);
 }
 std::sort(sorted.begin(),sorted.end());
 if(std::adjacent_find(sorted.begin(),sorted.end())!=sorted.end()) {p.status="Duplicate GPU tokens use CPU";return;}
 try {
  p.api.reset(new Api);
  const auto all=devices(*p.api,allow_cpu_for_tests);
  std::string failures;
  for(const auto& device:all) {
   if(!device.info.available)continue;
   try{p.initialize(device);return;}
   catch(const std::exception& error){failures+=device.info.name+": "+error.what()+"; ";p.release();}
  }
  p.status=failures.empty()?"No usable OpenCL GPU was detected; CPU remains available":failures;
 }catch(const std::exception& error){p.status=error.what();p.release();}
}
GpuEngine::~GpuEngine()=default;
bool GpuEngine::available() const{return impl->function!=nullptr;}
std::string GpuEngine::name() const{return impl->device_name;}
std::string GpuEngine::status() const{return impl->status;}
size_t GpuEngine::max_span(size_t row_bytes) const{return impl->capacity(row_bytes);}

GpuResult GpuEngine::generate(const std::vector<uint32_t>& digits,unsigned length,size_t span,
 unsigned required,bool no_repeat,const std::string& prefix,const std::string& suffix,
 const std::atomic<bool>* stop,unsigned target_percent) {
 auto& p=*impl;
 if(!available())throw std::runtime_error(p.status);
 if(!length||length>128||digits.size()!=length||required>15||!span||span>hard_span||
    prefix.size()>4096||suffix.size()>4096||target_percent<1||target_percent>100)
  throw std::invalid_argument("Invalid bounded OpenCL batch parameters");
 for(auto digit:digits)if(digit>=p.tokens.size())throw std::invalid_argument("OpenCL seed digit is out of range");
 uint64_t carry=span-1;
 for(auto digit:digits)carry=(digit+carry)/p.tokens.size();
 if(carry)throw std::invalid_argument("OpenCL batch crosses the token-length boundary");
 const size_t row_bytes=prefix.size()+length*p.width+suffix.size()+1;
 // The adaptive value can decrease after a caller selected a span; enforce
 // actual memory bounds here rather than treating timing guidance as a quota.
 const auto timing_span=p.dynamic_span;p.dynamic_span=hard_span;
 const auto memory_span=p.capacity(row_bytes);p.dynamic_span=timing_span;
 if(span>memory_span)throw std::invalid_argument("OpenCL batch exceeds bounded buffer budget");
 while(std::chrono::steady_clock::now()<p.next_launch) {
  if(stop&&stop->load())return {};
  const auto remaining=p.next_launch-std::chrono::steady_clock::now();
  std::this_thread::sleep_for((std::min)(remaining,
   std::chrono::steady_clock::duration(std::chrono::milliseconds(10))));
 }
 if(stop&&stop->load())return {};
 p.set_affix(p.prefix_buffer,p.cached_prefix,prefix);p.set_affix(p.suffix_buffer,p.cached_suffix,suffix);
 const size_t bytes=span*row_bytes;
 if(p.rows_capacity<bytes){p.release_mem(p.rows_buffer);p.rows_buffer=p.buffer(mem_write_only,bytes);p.rows_capacity=bytes;}
 if(p.flags_capacity<span){p.release_mem(p.flags_buffer);p.flags_buffer=p.buffer(mem_write_only,span);p.flags_capacity=span;}
 check(p.api->EnqueueWriteBuffer(p.queue,p.digits_buffer,true_value,0,length*sizeof(uint_t),digits.data(),0,nullptr,nullptr),"Cannot transfer OpenCL rank seed");
 p.arg(0,p.table_buffer);p.arg(1,p.masks_buffer);p.arg(2,p.digits_buffer);
 p.arg(3,uint_t(length));p.arg(4,uint_t(p.width));p.arg(5,uint_t(p.tokens.size()));p.arg(6,uint_t(span));
 p.arg(7,uint_t(required));p.arg(8,uint_t(no_repeat));p.arg(9,uint_t(classes(prefix)|classes(suffix)));
 p.arg(10,p.prefix_buffer);p.arg(11,uint_t(prefix.size()));p.arg(12,p.suffix_buffer);
 p.arg(13,uint_t(suffix.size()));p.arg(14,uint_t(row_bytes));p.arg(15,p.rows_buffer);p.arg(16,p.flags_buffer);
 event timing=nullptr;
 std::vector<unsigned char> rows(bytes),flags(span);
 const auto begin=std::chrono::steady_clock::now();
 check(p.api->EnqueueNDRangeKernel(p.queue,p.function,1,nullptr,&span,nullptr,0,nullptr,&timing),"Cannot launch OpenCL generation");
 try {
  check(p.api->EnqueueReadBuffer(p.queue,p.rows_buffer,true_value,0,bytes,rows.data(),0,nullptr,nullptr),"Cannot read OpenCL rows");
  check(p.api->EnqueueReadBuffer(p.queue,p.flags_buffer,true_value,0,span,flags.data(),0,nullptr,nullptr),"Cannot read OpenCL flags");
  ulong_t started=0,ended=0;double device_seconds=0;
  if(timing&&p.api->GetEventProfilingInfo(timing,profiling_command_start,sizeof(started),&started,nullptr)==success&&
     p.api->GetEventProfilingInfo(timing,profiling_command_end,sizeof(ended),&ended,nullptr)==success&&ended>=started)
   device_seconds=double(ended-started)/1e9;
  if(timing){p.api->ReleaseEvent(timing);timing=nullptr;}
  const auto end=std::chrono::steady_clock::now();
  const double duration=device_seconds>0?device_seconds:std::chrono::duration<double>(end-begin).count();
  // Aim below 50 ms, leave scheduling margin and change by at most 2x/launch.
  if(duration>0) {
   const double target=double(span)*.025/duration;
   const size_t lower=(std::max)(size_t(1),span/2),upper=(std::min)(hard_span,span*2);
   p.dynamic_span=target<lower?lower:(target>upper?upper:static_cast<size_t>(target));
   if(duration>.1)p.dynamic_span=(std::max)(size_t(1),static_cast<size_t>(double(span)*.025/duration));
  }
  const double pause=duration*(100.-target_percent)/target_percent;
  p.next_launch=end+std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<double>(pause));
  if(!p.verified)p.verify(digits,length,span,required,no_repeat,prefix,suffix,row_bytes,rows,flags);
  GpuResult result;result.span=span;result.device_seconds=device_seconds;
  for(auto flag:flags){if(flag>1)throw std::runtime_error("OpenCL returned an invalid row flag");result.lines+=flag;}
  result.text.reserve(static_cast<size_t>(result.lines)*row_bytes);
  for(size_t i=0;i<span;++i)if(flags[i])result.text.append(reinterpret_cast<const char*>(rows.data()+i*row_bytes),row_bytes);
  return result;
 }catch(...) {if(timing)p.api->ReleaseEvent(timing);throw;}
}
} // namespace pg
