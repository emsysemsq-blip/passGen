#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sched.h>
#include <sys/statvfs.h>
#include <unistd.h>
#endif

namespace pg {
struct ResourceSnapshot {
 unsigned logical_cpus=1;
 double cpu_capacity=1;
 bool cpu_known=false;
 uint64_t memory_available_bytes=0;
 bool memory_known=false;
 uint64_t disk_available_bytes=0;
 bool disk_known=false;
 bool cpu_limited=false, memory_limited=false;
};
struct ResourcePlan {
 unsigned percent=70, workers=1;
 double cpu_fraction=.7;
 uint64_t memory_budget_bytes=0;
 uint64_t buffer_bytes_per_worker=1024*1024;
 uint64_t disk_budget_bytes=0;
 bool memory_sufficient=true;
};
namespace resource_detail {
#ifdef _WIN32
template<class Function> inline Function windows_function(HMODULE module,const char* name) {
 auto address=GetProcAddress(module,name); Function result=nullptr;
 static_assert(sizeof(result)==sizeof(address),"Windows function pointer size");
 std::memcpy(&result,&address,sizeof(result));
 return result;
}
#endif
inline uint64_t multiply_saturated(uint64_t a,uint64_t b) {
 const uint64_t maximum=(std::numeric_limits<uint64_t>::max)();
 return b&&a>maximum/b?maximum:a*b;
}
inline uint64_t percentage(uint64_t value,unsigned percent) {
 // Divide before multiplying so even an unlimited filesystem cannot overflow.
 return value/100*percent+value%100*percent/100;
}
inline bool unsigned_value(const std::string& text,uint64_t& value) {
 std::istringstream input(text); std::string trailing;
 if(text.empty()||text.find_first_not_of(" \t\r\n0123456789")!=std::string::npos)return false;
 return bool(input>>value)&&!(input>>trailing);
}
inline std::string read_text(const std::filesystem::path& file) {
 std::ifstream input(file); std::ostringstream text;
 if(input)text<<input.rdbuf();
 return text.str();
}
inline double parse_cpu_max(const std::string& text) {
 std::istringstream input(text); std::string quota,period,trailing; uint64_t q=0,p=0;
 if(!(input>>quota>>period)||(input>>trailing)||!unsigned_value(quota,q)||
    !unsigned_value(period,p)||!q||!p)return 0;
 return double(q)/double(p);
}
inline void apply_cpu_quota(ResourceSnapshot& value,double cores) {
 if(std::isfinite(cores)&&cores>0&&cores<value.cpu_capacity) {
  value.cpu_capacity=cores; value.cpu_limited=true; value.cpu_known=true;
 }
}
inline void apply_memory_limit(ResourceSnapshot& value,const std::string& limit_text,
                               const std::string& usage_text) {
 uint64_t limit=0,usage=0;
 if(!unsigned_value(limit_text,limit)||!unsigned_value(usage_text,usage))return;
 const uint64_t remaining=usage>=limit?0:limit-usage;
 if(!value.memory_known||remaining<value.memory_available_bytes) {
  value.memory_available_bytes=remaining; value.memory_limited=true; value.memory_known=true;
 }
}
inline std::string decode_mount(const std::string& value) {
 std::string result;
 for(size_t i=0;i<value.size();++i) {
  if(value[i]=='\\'&&i+3<value.size()&&value[i+1]>='0'&&value[i+1]<='7'&&
     value[i+2]>='0'&&value[i+2]<='7'&&value[i+3]>='0'&&value[i+3]<='7') {
   result+=char((value[i+1]-'0')*64+(value[i+2]-'0')*8+value[i+3]-'0'); i+=3;
  }else result+=value[i];
 }
 return result;
}
inline bool has_controller(const std::string& list,const std::string& controller) {
 return (","+list+",").find(","+controller+",")!=std::string::npos;
}
inline std::filesystem::path cgroup_path(const std::string& mount,const std::string& root,
                                       const std::string& group) {
 std::string relative=group;
 if(root!="/"&&group==root)relative.clear();
 else if(root!="/"&&group.compare(0,root.size()+1,root+"/")==0)relative=group.substr(root.size());
 // A cgroup namespace can show '/' even when the mount's root is a host path.
 while(!relative.empty()&&relative.front()=='/')relative.erase(relative.begin());
 auto boundary=std::filesystem::path(mount).lexically_normal();
 if(relative.empty())return boundary;
 auto result=(boundary/relative).lexically_normal();
 auto r=result.begin(),b=boundary.begin();
 for(;b!=boundary.end()&&r!=result.end()&&*r==*b;++b,++r){}
 return b==boundary.end()?result:boundary;
}
#ifndef _WIN32
inline void apply_cgroups(ResourceSnapshot& value) {
 struct Group { std::string controllers,path; };
 std::vector<Group> groups;
 std::istringstream membership(read_text("/proc/self/cgroup")); std::string line;
 while(std::getline(membership,line)) {
  auto first=line.find(':'),second=first==std::string::npos?first:line.find(':',first+1);
  if(second!=std::string::npos)groups.push_back({line.substr(first+1,second-first-1),line.substr(second+1)});
 }
 std::istringstream mounts(read_text("/proc/self/mountinfo"));
 while(std::getline(mounts,line)) {
  std::istringstream fields(line); std::vector<std::string> parts; std::string field;
  while(fields>>field)parts.push_back(field);
  auto separator=std::find(parts.begin(),parts.end(),"-");
  if(parts.size()<10||separator==parts.end()||parts.end()-separator<4)continue;
  const bool unified=separator[1]=="cgroup2";
  if(!unified&&separator[1]!="cgroup")continue;
  const std::string root=decode_mount(parts[3]),mount=decode_mount(parts[4]);
  const std::string controllers=separator[3];
  for(const auto& group:groups) {
   const bool cpu=unified?group.controllers.empty():
    has_controller(controllers,"cpu")&&has_controller(group.controllers,"cpu");
   const bool memory=unified?group.controllers.empty():
    has_controller(controllers,"memory")&&has_controller(group.controllers,"memory");
   if(!cpu&&!memory)continue;
   auto current=cgroup_path(mount,root,group.path);
   const auto boundary=std::filesystem::path(mount).lexically_normal();
   for(;;) {
    if(cpu) {
     const auto quota=unified?parse_cpu_max(read_text(current/"cpu.max")):
       parse_cpu_max(read_text(current/"cpu.cfs_quota_us")+" "+read_text(current/"cpu.cfs_period_us"));
     apply_cpu_quota(value,quota);
    }
    if(memory)apply_memory_limit(value,read_text(current/(unified?"memory.max":"memory.limit_in_bytes")),
      read_text(current/(unified?"memory.current":"memory.usage_in_bytes")));
    if(current==boundary||current==current.root_path()||current.empty())break;
    current=current.parent_path();
   }
  }
 }
}
#endif
inline std::filesystem::path disk_directory(const std::string& output) {
 std::error_code error;
 auto path=output.empty()?std::filesystem::path("."):std::filesystem::u8path(output);
 if(!std::filesystem::is_directory(path,error))path=path.parent_path();
 if(path.empty())path=".";
 path=std::filesystem::absolute(path,error);
 if(error)return {};
 while(!std::filesystem::is_directory(path,error)) {
  auto parent=path.parent_path();
  if(parent==path||parent.empty())return {};
  path=parent; error.clear();
 }
 return path;
}
} // namespace resource_detail

inline ResourceSnapshot detect_resources(const std::string& output_path={}) {
 ResourceSnapshot result;
#ifdef _WIN32
 SYSTEM_INFO system{}; GetNativeSystemInfo(&system);
 result.logical_cpus=system.dwNumberOfProcessors?system.dwNumberOfProcessors:1;
 result.cpu_known=system.dwNumberOfProcessors!=0;
 using ActiveCount=DWORD(WINAPI*)(WORD);
 using ProcessGroups=BOOL(WINAPI*)(HANDLE,PUSHORT,PUSHORT);
 auto kernel=GetModuleHandleW(L"kernel32.dll");
 auto active=resource_detail::windows_function<ActiveCount>(kernel,"GetActiveProcessorCount");
 auto groups=resource_detail::windows_function<ProcessGroups>(kernel,"GetProcessGroupAffinity");
 unsigned allowed=0;
 if(active&&groups) {
  USHORT count=64; USHORT ids[64]{};
  if(groups(GetCurrentProcess(),&count,ids))
   for(USHORT i=0;i<count;++i)allowed+=active(ids[i]);
 }
 if(allowed)result.logical_cpus=allowed;
 DWORD_PTR process_mask=0,system_mask=0;
 if(GetProcessAffinityMask(GetCurrentProcess(),&process_mask,&system_mask)&&process_mask) {
  unsigned count=0; for(auto mask=process_mask;mask;mask>>=1)count+=unsigned(mask&1);
  // A non-default mask is a real restriction; an all-CPU mask describes only
  // the primary group on older Windows and must not hide other allowed groups.
  if(!allowed||process_mask!=system_mask) {
   result.cpu_limited=count<result.logical_cpus;
   result.logical_cpus=count;
  }
  result.cpu_known=true;
 }
 result.cpu_capacity=result.logical_cpus;
 MEMORYSTATUSEX memory{}; memory.dwLength=sizeof(memory);
 if(GlobalMemoryStatusEx(&memory)) {
  result.memory_available_bytes=memory.ullAvailPhys; result.memory_known=true;
 }
 auto directory=resource_detail::disk_directory(output_path);
 ULARGE_INTEGER available{};
 if(!directory.empty()&&GetDiskFreeSpaceExW(directory.c_str(),&available,nullptr,nullptr)) {
  result.disk_available_bytes=available.QuadPart; result.disk_known=true;
 }
#else
 long online=sysconf(_SC_NPROCESSORS_ONLN);
 if(online>0){result.logical_cpus=unsigned(online);result.cpu_known=true;}
 cpu_set_t affinity; CPU_ZERO(&affinity);
 if(sched_getaffinity(0,sizeof(affinity),&affinity)==0) {
  const int allowed=CPU_COUNT(&affinity);
  if(allowed>0) {
   result.cpu_limited=unsigned(allowed)<result.logical_cpus;
   result.logical_cpus=unsigned(allowed); result.cpu_known=true;
  }
 }
 result.cpu_capacity=result.logical_cpus;
 std::istringstream meminfo(resource_detail::read_text("/proc/meminfo"));
 std::string line; uint64_t available=0,free=0,buffers=0,cached=0; bool found=false;
 while(std::getline(meminfo,line)) {
  std::istringstream fields(line); std::string key,unit; uint64_t value=0;
  if(!(fields>>key>>value>>unit)||unit!="kB")continue;
  if(key=="MemAvailable:"){available=value;found=true;}
  else if(key=="MemFree:")free=value;
  else if(key=="Buffers:")buffers=value;
  else if(key=="Cached:")cached=value;
 }
 if(found||free) {
  result.memory_available_bytes=resource_detail::multiply_saturated(found?available:free+buffers+cached,1024);
  result.memory_known=true;
 }
 resource_detail::apply_cgroups(result);
 auto directory=resource_detail::disk_directory(output_path);
 struct statvfs disk{};
 if(!directory.empty()&&statvfs(directory.c_str(),&disk)==0) {
  result.disk_available_bytes=resource_detail::multiply_saturated(disk.f_bavail,disk.f_frsize);
  result.disk_known=true;
 }
#endif
 return result;
}

inline ResourcePlan plan_resources(const ResourceSnapshot& snapshot,unsigned percent=70,unsigned max_workers=32) {
 ResourcePlan result;
 result.percent=(std::max)(1u,(std::min)(100u,percent));
 const double capacity=std::isfinite(snapshot.cpu_capacity)&&snapshot.cpu_capacity>0?snapshot.cpu_capacity:1;
 const double budget=capacity*result.percent/100;
 const unsigned limit=(std::max)(1u,(std::min)(32u,max_workers));
 result.workers=budget>=limit?limit:(std::max)(1u,unsigned(std::ceil(budget)));
 result.cpu_fraction=(std::min)(1.,budget/result.workers);
 if(snapshot.memory_known) {
  result.memory_budget_bytes=resource_detail::percentage(snapshot.memory_available_bytes,result.percent);
  constexpr uint64_t base_reserve=2*1024*1024,thread_reserve=2*1024*1024,minimum_buffer=64*1024;
  const uint64_t reserve=base_reserve+thread_reserve*result.workers;
  if(result.memory_budget_bytes<reserve+minimum_buffer*result.workers) {
   result.memory_sufficient=false; result.buffer_bytes_per_worker=0;
  }else result.buffer_bytes_per_worker=(std::min)(uint64_t(4*1024*1024),
    (result.memory_budget_bytes-reserve)/result.workers);
 }
 if(snapshot.disk_known)result.disk_budget_bytes=resource_detail::percentage(snapshot.disk_available_bytes,result.percent);
 return result;
}
} // namespace pg
