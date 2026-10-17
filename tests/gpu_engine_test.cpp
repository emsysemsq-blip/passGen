#include "engine.hpp"
#include "gpu.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <unordered_set>

namespace fs=std::filesystem;
static unsigned checks=0;
static void check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
static std::string read(const fs::path& file){std::ifstream input(file,std::ios::binary);return {(std::istreambuf_iterator<char>(input)),{}};}
static void write(const fs::path& file,const std::string& text,bool append=false){std::ofstream output(file,std::ios::binary|(append?std::ios::app:std::ios::trunc));output<<text;if(!output)throw std::runtime_error("Cannot write fixture");}
static uint64_t fingerprint(const std::string& text){uint64_t hash=14695981039346656037ull;for(unsigned char c:text){hash^=c;hash*=1099511628211ull;}return hash;}
static pg::ResourceSnapshot resources(uint64_t diskBudget=64ull*1024*1024){pg::ResourceSnapshot value;value.logical_cpus=4;value.cpu_capacity=4;value.cpu_known=true;value.memory_available_bytes=256ull*1024*1024;value.memory_known=true;value.disk_available_bytes=(diskBudget*100+69)/70;value.disk_known=true;return value;}
static void unique(const std::string& text){std::istringstream input(text);std::string line;std::unordered_set<std::string> seen;bool all=true;while(std::getline(input,line))all&=seen.insert(line).second;check(all,"GPU and fallback paths never write duplicate full lines");}
static pg::Index normalizedTotal(pg::Config config){std::unordered_set<std::string> seen;config.tokens.erase(std::remove_if(config.tokens.begin(),config.tokens.end(),[&](const std::string& text){return !seen.insert(text).second;}),config.tokens.end());return pg::total(config);}
static std::string cpuReference(pg::Config config,const fs::path& file){config.compute_mode=pg::ComputeMode::Cpu;config.resource_percent=0;pg::Progress progress;pg::run(config,file.u8string(),false,progress);check(progress.scanned.load()==normalizedTotal(config)&&!progress.gpu_used.load(),"Explicit CPU reference completes without an accelerator");return read(file);}
static void parity(const fs::path& folder,const std::string& label,pg::Config config,const pg::GpuTestOptions& testing,bool expectGpu){
 const auto expected=cpuReference(config,folder/(label+"-cpu.txt"));config.compute_mode=pg::ComputeMode::GpuPreferred;pg::Progress progress;
 pg::run(config,(folder/(label+"-gpu.txt")).u8string(),false,progress,{},nullptr,&testing);
 check(read(folder/(label+"-gpu.txt"))==expected,"Accelerated or automatic-fallback file equals the exact CPU file");
 check(progress.scanned.load()==normalizedTotal(config),"Accelerator integration scans the complete arbitrary-precision range");
 check(progress.gpu_used.load()==expectGpu,"Progress truthfully identifies whether an OpenCL kernel contributed output");
 if(expectGpu)check(progress.gpu_scanned.load()>0,"GPU telemetry counts actually completed OpenCL candidate work");
 unique(expected);
}
static void checkpoint(const fs::path& file,unsigned version,const pg::Index& rank,unsigned length,unsigned compute=2){
 write(file,"");std::ostringstream body;body<<"PASSGEN_"<<version<<'\n'<<rank<<" 0 0 "<<fingerprint("")<<'\n'<<length<<' '<<length<<" 2 0 0 0 0 0 0 0";
 if(version>=5)body<<' '<<compute;
 body<<"\n- -\n2\n61\n62\n";const auto text=body.str();write(fs::u8path(file.u8string()+".pgstate"),text+"CHECK "+std::to_string(fingerprint(text))+"\n");
}
int main(int argc,char** argv){
 const bool required=argc>1&&std::string(argv[1])=="--require-opencl";
 const auto folder=fs::temp_directory_path()/("passgen-gpu-engine-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));fs::create_directories(folder);
 try{
  const auto production=pg::query_gpu();pg::Config config;config.tokens={"a","b"};config.max_length=3;config.compute_mode=pg::ComputeMode::Automatic;
  pg::Progress fallback;pg::run(config,(folder/"production.txt").u8string(),false,fallback);
  check(read(folder/"production.txt")=="a\nb\naa\nab\nba\nbb\naaa\naab\naba\nabb\nbaa\nbab\nbba\nbbb\n","Production automatic discovery preserves complete ordered output");
  if(production.devices.empty())check(!fallback.gpu_used.load()&&fallback.gpu_scanned.load()==0,"A CPU OpenCL runtime is never silently used as a production GPU");
  pg::GpuEngine probe({{"a",1},{"b",1}},4*1024*1024,true);
  if(!probe.available()){
   if(required)throw std::runtime_error("--require-opencl requested real kernel integration: "+probe.status());
   fs::remove_all(folder);std::cout<<"SKIP actual OpenCL integration: "<<probe.status()<<"; "<<checks<<" automatic fallback checks passed\n";return 0;
  }
  std::cout<<"Actual OpenCL integration device: "<<probe.name()<<" (explicit CPU-device test hook)\n";
  pg::GpuTestOptions testing;testing.allow_cpu_opencl=true;testing.skip_speed_selection=true;testing.max_batch_candidates=256;
  config={};config.tokens={"a","A","1","!"};config.min_length=2;config.max_length=4;config.prefix="p";config.suffix="9";config.lower=config.upper=config.digit=config.symbol=true;config.no_repeat=true;config.threads=3;
  parity(folder,"classes-and-affixes",config,testing,true);
  config={};config.tokens={u8"آ",u8"ب",u8"ی"};config.character_lengths=true;config.min_length=3;config.max_length=5;config.prefix=u8"پ";config.suffix="!";config.no_repeat=true;
  parity(folder,"persian",config,testing,true);
  config={};config.tokens={u8"😀",u8"🚀"};config.max_length=4;parity(folder,"emoji",config,testing,true);
  config={};config.tokens={"a",u8"ب",u8"😀"};config.max_length=4;parity(folder,"mixed-utf8-fallback",config,testing,false);
  config={};config.tokens={"a","aa","b"};config.max_length=4;parity(folder,"ambiguous-word-fallback",config,testing,false);
  config={};config.tokens={"aa",u8"ب"};config.max_length=4;parity(folder,"equal-byte-word-tokens",config,testing,true);
  config.character_lengths=true;parity(folder,"unequal-scalar-length-fallback",config,testing,false);
  config={};config.tokens={"a","a","b"};config.max_length=5;parity(folder,"deduplicated-input",config,testing,true);

  // Resource quota intersects a filtered GPU batch. The checkpoint must identify
  // the raw rank before the first unwritten accepted row, not its compacted offset.
  config={};config.tokens={"a","A"};config.max_length=3;config.upper=true;config.resource_percent=70;config.compute_mode=pg::ComputeMode::GpuPreferred;
  const auto quotaFile=folder/"filtered-quota.txt";auto tiny=resources(3);pg::Progress limited;
  pg::run(config,quotaFile.u8string(),false,limited,{},&tiny,&testing);
  check(read(quotaFile)=="A\n"&&limited.scanned.load()==3,"GPU disk quota saves the precise raw rank including rejected candidates");
  check(limited.disk_limit&&limited.bytes.load()==2&&limited.written.load()==1,"GPU disk quota commits only complete rows within its byte budget");
  write(quotaFile,"uncommitted GPU tail\n",true);auto ample=resources();pg::Progress resumed;bool restored=false;
  pg::run({},quotaFile.u8string(),true,resumed,[&](const pg::Config& active){restored=active.compute_mode==pg::ComputeMode::GpuPreferred&&active.resource_percent==70;},&ample,&testing);
  check(restored&&resumed.scanned.load()==resumed.total.load(),"Checkpoint resume restores compute preference and resource policy");
  const auto quotaExpected=cpuReference(config,folder/"quota-cpu.txt");check(read(quotaFile)==quotaExpected,"GPU crash-tail replay and filtered quota resume equal uninterrupted CPU output");unique(read(quotaFile));
  pg::Progress completed;pg::run({},quotaFile.u8string(),true,completed,{},&ample,&testing);check(read(quotaFile)==quotaExpected,"Completed GPU resume is idempotent");

  config={};config.tokens={"a","b"};config.min_length=config.max_length=20;config.compute_mode=pg::ComputeMode::GpuPreferred;
  const auto stopFile=folder/"stop.txt";pg::Progress stopped;std::atomic<bool> done{false};
  std::thread stopper([&]{while(!done&&!stopped.scanned.load())std::this_thread::yield();if(!done)stopped.stop=true;});
  try{pg::run(config,stopFile.u8string(),false,stopped,{},nullptr,&testing);}catch(...){done=true;stopper.join();throw;}done=true;stopper.join();
  check(stopped.scanned.load()>0&&stopped.scanned.load()<stopped.total.load(),"Stop after a nonempty GPU commit leaves a partial exact checkpoint");
  check(stopped.gpu_used.load()&&stopped.gpu_scanned.load()>0,"The stopped job actually executed OpenCL work");
  const auto prefix=read(stopFile);write(stopFile,"crash after committed GPU batch",true);testing.max_batch_candidates=4096;pg::Progress recovered;
  pg::run({},stopFile.u8string(),true,recovered,{},nullptr,&testing);const auto expected=cpuReference(config,folder/"stop-cpu.txt");
  check(read(stopFile)==expected&&expected.compare(0,prefix.size(),prefix)==0,"GPU stop and resume retain committed bytes without lost or repeated lines");
  check(recovered.scanned.load()==recovered.total.load()&&recovered.written.load()==1048576,"GPU stop/resume commits the full 2^20 ordered result");unique(read(stopFile));

  // Inject a backend failure only through the explicit, non-production test hook.
  // At least one earlier GPU range is already committed before CPU replay begins.
  const auto failedFile=folder/"kernel-failure.txt";auto failing=testing;failing.max_batch_candidates=256;failing.fail_after_batches=1;pg::Progress failed;
  pg::run(config,failedFile.u8string(),false,failed,{},nullptr,&failing);
  check(failed.gpu_used.load()&&failed.gpu_scanned.load()>0,"A backend failure follows actual previously committed OpenCL work");
  check(failed.compute_status.get().find("CPU fallback after GPU error")!=std::string::npos,"Runtime kernel failure exposes the CPU fallback reason");
  check(read(failedFile)==expected&&failed.scanned.load()==failed.total.load(),"A failed GPU range is replayed by CPU with identical bytes and complete rank coverage");
  unique(read(failedFile));

  auto automatic=config;automatic.compute_mode=pg::ComputeMode::Automatic;auto calibrated=testing;calibrated.skip_speed_selection=false;pg::Progress selected;
  const auto automaticFile=folder/"calibrated-auto.txt";
  pg::run(automatic,automaticFile.u8string(),false,selected,{},nullptr,&calibrated);
  check(selected.gpu_available.load(),"Automatic calibration discovers the explicitly permitted OpenCL test device");
  check(selected.gpu_used.load()||selected.compute_status.get().find("calibration did not improve")!=std::string::npos,"Automatic profiling selects a measured backend instead of unconditionally enabling OpenCL");
  check(read(automaticFile)==expected&&selected.scanned.load()==selected.total.load(),"Transfer-inclusive backend calibration leaves exact generation order and bytes unchanged");

  const auto huge=folder/"huge.txt";pg::Index count=1;for(unsigned i=0;i<128;++i)count*=2;checkpoint(huge,5,count-3,128);pg::Progress hugeProgress;
  pg::run({},huge.u8string(),true,hugeProgress,{},nullptr,&testing);
  const auto hugeExpected=std::string(126,'b')+"ab\n"+std::string(127,'b')+"a\n"+std::string(128,'b')+"\n";
  check(read(huge)==hugeExpected&&hugeProgress.scanned.load()==count,"The engine transfers a 128-bit saved rank into exact GPU seed digits");
  check(hugeProgress.gpu_scanned.load()==3&&hugeProgress.gpu_used.load(),"The final three candidates beyond 64-bit indexing run through OpenCL");

  const auto old=folder/"legacy.txt";checkpoint(old,4,0,2);pg::Progress legacy;bool cpuPolicy=false;
  pg::run({},old.u8string(),true,legacy,[&](const pg::Config& active){cpuPolicy=active.compute_mode==pg::ComputeMode::Cpu;},nullptr,&testing);
  check(cpuPolicy&&!legacy.gpu_used.load()&&read(old)=="aa\nab\nba\nbb\n","Legacy checkpoints preserve the CPU policy and exact old alphabet");
  fs::remove_all(folder);std::cout<<"PASS: "<<checks<<" actual OpenCL engine checks\n";return 0;
 }catch(const std::exception& error){std::cerr<<"FAIL after "<<checks<<" checks: "<<error.what()<<" | artifacts "<<folder<<'\n';return 1;}
}
