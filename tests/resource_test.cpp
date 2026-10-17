#include "resources.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
static void check(bool value,const char* message) {if(!value)throw std::runtime_error(message);}
static bool approximately(double a,double b){return std::abs(a-b)<1e-9;}
int main(){try{
 using namespace pg;
 using namespace pg::resource_detail;
 check(approximately(parse_cpu_max("400000 100000\n"),4),"cgroup v2 CPU quota");
 check(approximately(parse_cpu_max("50000 100000"),.5),"Fractional CPU quota");
 for(const auto* text:{"max 100000","-1 100000","0 100000","50000 0","","1 2 junk"})
  check(parse_cpu_max(text)==0,"Unlimited or invalid CPU quota must be ignored");
 check(percentage((std::numeric_limits<uint64_t>::max)(),70)==12912720851596686130ull,
       "Large resource percentage must not overflow");
 check(multiply_saturated((std::numeric_limits<uint64_t>::max)(),2)==(std::numeric_limits<uint64_t>::max)(),
       "Filesystem byte multiplication must saturate");
 ResourceSnapshot snapshot; snapshot.logical_cpus=8;snapshot.cpu_capacity=8;snapshot.cpu_known=true;
 apply_cpu_quota(snapshot,4);apply_cpu_quota(snapshot,6);
 check(approximately(snapshot.cpu_capacity,4)&&snapshot.cpu_limited,"Strictest ancestor CPU limit wins");
 auto plan=plan_resources(snapshot);
 check(plan.workers==3&&approximately(plan.cpu_fraction,2.8/3),"70 percent of four effective cores");
 snapshot.cpu_capacity=.5;plan=plan_resources(snapshot);
 check(plan.workers==1&&approximately(plan.cpu_fraction,.35),"Fractional quota uses one paced worker");
 snapshot.cpu_capacity=128;plan=plan_resources(snapshot);
 check(plan.workers==32&&approximately(plan.cpu_fraction,1),"Worker count remains bounded on a large host");
 snapshot.cpu_capacity=8;plan=plan_resources(snapshot,70,2);
 check(plan.workers==2&&approximately(plan.cpu_fraction,1),"Explicit worker ceiling");
 plan=plan_resources(snapshot,1000,0);
 check(plan.workers==1&&plan.percent==100,"Invalid planner parameters are clamped");
 snapshot.cpu_capacity=std::numeric_limits<double>::quiet_NaN();
 check(plan_resources(snapshot).workers==1,"Unknown CPU fallback");
 snapshot.cpu_capacity=4;snapshot.memory_known=true;snapshot.memory_available_bytes=1024ull*1024*1024;
 apply_memory_limit(snapshot,"536870912","134217728");
 check(snapshot.memory_available_bytes==402653184&&snapshot.memory_limited,"Cgroup remaining memory");
 apply_memory_limit(snapshot,"1073741824","0");
 check(snapshot.memory_available_bytes==402653184,"Looser memory ancestor does not increase capacity");
 auto previous=snapshot.memory_available_bytes;
 apply_memory_limit(snapshot,"max","100");apply_memory_limit(snapshot,"bad","100");
 check(snapshot.memory_available_bytes==previous,"Unlimited/invalid memory limits are ignored");
 plan=plan_resources(snapshot);
 check(plan.memory_sufficient&&plan.buffer_bytes_per_worker==4*1024*1024,
       "Output buffers are bounded on a memory-rich system");
 check(plan.buffer_bytes_per_worker*plan.workers<=plan.memory_budget_bytes,"Buffers stay within RAM budget");
 snapshot.memory_available_bytes=20*1024*1024;plan=plan_resources(snapshot);
 check(plan.memory_sufficient&&plan.buffer_bytes_per_worker<4*1024*1024,
       "Buffers shrink on memory-constrained systems");
 apply_memory_limit(snapshot,"100","200");plan=plan_resources(snapshot);
 check(snapshot.memory_available_bytes==0&&!plan.memory_sufficient&&plan.buffer_bytes_per_worker==0,
       "Exhausted memory quota declines generation");
 snapshot.disk_known=true;snapshot.disk_available_bytes=1001;
 check(plan_resources(snapshot).disk_budget_bytes==700,"Disk budget rounds down");
 snapshot.memory_known=false;plan=plan_resources(snapshot);
 check(plan.memory_sufficient&&plan.buffer_bytes_per_worker==1024*1024,"Unknown memory conservative buffer");
 check(has_controller("cpu,cpuacct","cpu")&&!has_controller("cpuacct","cpu"),"Exact v1 controller matching");
 check(decode_mount("/test\\040folder")=="/test folder","Mount paths decode spaces");
 check(cgroup_path("/cgroup","/host/group","/host/group/job")==std::filesystem::path("/cgroup/job"),
       "Host-relative cgroup mapping");
 check(cgroup_path("/cgroup","/..","/")==std::filesystem::path("/cgroup"),
       "Namespaced cgroup root mapping");
 check(cgroup_path("/cgroup","/","/../../etc")==std::filesystem::path("/cgroup"),
       "Malformed cgroup paths cannot escape mount");
 auto detected=detect_resources((std::filesystem::temp_directory_path()/"passgen-missing-resource-dir"/"nested"/"output.txt").u8string());
 check(detected.logical_cpus>=1&&std::isfinite(detected.cpu_capacity)&&detected.cpu_capacity>0,
       "Detected effective CPU capacity is valid");
 check(detected.disk_known&&detected.disk_available_bytes>0,"Detect disk at nearest existing output parent");
 check(detected.memory_known,"Available physical memory detected");
 auto detected_plan=plan_resources(detected);
 check(detected_plan.workers>=1&&detected_plan.workers<=32&&detected_plan.cpu_fraction>0&&detected_plan.cpu_fraction<=1,
       "Detected resources produce bounded worker plan");
 std::cout<<"PASS: resource quotas, memory/disk budgets, overflow, CPU pacing plan, live detection\n"
          <<"Detected: "<<detected.logical_cpus<<" logical CPUs, "<<detected.cpu_capacity<<" effective cores, "
          <<detected.memory_available_bytes<<" available RAM bytes, "<<detected.disk_available_bytes
          <<" free disk bytes; workers="<<detected_plan.workers<<", CPU fraction="<<detected_plan.cpu_fraction<<'\n';
 return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
