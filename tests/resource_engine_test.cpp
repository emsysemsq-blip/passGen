#include "engine.hpp"
#include <chrono>
#include <ctime>
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
static void write(const fs::path& file,const std::string& text,bool append=false){std::ofstream output(file,std::ios::binary|(append?std::ios::app:std::ios::trunc));output<<text;if(!output)throw std::runtime_error("Cannot write test fixture");}
static uint64_t fingerprint(const std::string& text){uint64_t value=14695981039346656037ull;for(unsigned char byte:text){value^=byte;value*=1099511628211ull;}return value;}
static pg::ResourceSnapshot snapshot(uint64_t budget=64ull*1024*1024){
 pg::ResourceSnapshot value;value.logical_cpus=4;value.cpu_capacity=4;value.cpu_known=true;
 value.memory_available_bytes=256ull*1024*1024;value.memory_known=true;
 // floor(70% * ceil(budget / 70%)) is exactly budget for these fixtures.
 value.disk_available_bytes=(budget*100+69)/70;value.disk_known=true;return value;
}
static double process_seconds(){
#ifdef _WIN32
 FILETIME created{},exited{},kernel{},user{};
 if(!GetProcessTimes(GetCurrentProcess(),&created,&exited,&kernel,&user))throw std::runtime_error("Cannot measure process CPU time");
 ULARGE_INTEGER k{},u{};k.LowPart=kernel.dwLowDateTime;k.HighPart=kernel.dwHighDateTime;u.LowPart=user.dwLowDateTime;u.HighPart=user.dwHighDateTime;
 return double(k.QuadPart+u.QuadPart)/10000000.;
#else
 return double(std::clock())/CLOCKS_PER_SEC;
#endif
}
static std::string complete(pg::Config config,const fs::path& output){config.resource_percent=0;pg::Progress progress;pg::run(config,output.u8string(),false,progress);check(progress.scanned.load()==pg::total(config),"Unlimited reference completes");return read(output);}
static size_t prefix_bytes(const std::string& text,uint64_t budget){size_t end=0;for(;;){auto next=text.find('\n',end);if(next==std::string::npos||next+1>budget)return end;end=next+1;}}
static void unique_lines(const std::string& text){std::istringstream input(text);std::unordered_set<std::string> seen;std::string line;while(std::getline(input,line))check(seen.insert(line).second,"Resource-limited output lines remain unique");}
static void budget_case(const fs::path& folder,const std::string& label,pg::Config config,uint64_t budget,const std::string& expected){
 auto output=folder/(label+".txt");auto resources=snapshot(budget);config.resource_percent=70;pg::Progress limited;
 bool loaded=false;pg::run(config,output.u8string(),false,limited,[&](const pg::Config& active){loaded=active.resource_percent==70&&active.threads==3;},&resources);
 auto bytes=prefix_bytes(expected,budget);check(read(output)==expected.substr(0,bytes),"Disk quota writes the exact complete-line prefix");
 check(limited.bytes.load()==bytes&&fs::file_size(output)==bytes,"Disk quota byte counters match committed file");
 check(limited.disk_budget_bytes.load()==budget&&limited.active_workers.load()==3&&loaded,"Detected resource plan reaches the engine and loaded callback");
 check(limited.buffer_bytes.load()<=4ull*1024*1024&&limited.buffer_bytes.load()>0,"Worker buffers remain bounded");
 const bool finished=bytes==expected.size();check(limited.disk_limit.load()==!finished,"Disk quota reason agrees with incomplete output");
 if(!finished){
  auto ample=snapshot();pg::Progress resumed;bool restored=false;
  // A manual form config must not change the saved job's automatic policy.
  pg::run({},output.u8string(),true,resumed,[&](const pg::Config& active){restored=active.resource_percent==70;},&ample);
  check(restored&&resumed.scanned.load()==resumed.total.load(),"Resume restores automatic policy and completes");
  check(read(output)==expected,"Resume replays the first uncommitted candidate without loss or duplication");
 }
 pg::Progress again;auto ample=snapshot();pg::run({},output.u8string(),true,again,{},&ample);
 check(read(output)==expected&&again.scanned.load()==again.total.load(),"Completed automatic resume is idempotent");
}
static void legacy(const fs::path& output,unsigned version){
 write(output,"");std::ostringstream body;body<<"PASSGEN_"<<version<<"\n0 0 0 "<<fingerprint("")<<"\n1 2 2 0 0 0 0 0";
 if(version==3)body<<" 0";
 body<<"\n- -\n2\n61\n62\n";
 write(fs::u8path(output.u8string()+".pgstate"),body.str()+"CHECK "+std::to_string(fingerprint(body.str()))+"\n");
}
int main(){
 auto folder=fs::temp_directory_path()/("passgen-resource-engine-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));fs::create_directories(folder);
 try{
  pg::Config config;config.tokens={"a","b"};config.max_length=4;
  const auto expected=complete(config,folder/"reference.txt");
  for(uint64_t budget:{0ull,1ull,2ull,7ull,13ull})budget_case(folder,"small-"+std::to_string(budget),config,budget,expected);

  config={};config.tokens={"a","A"};config.max_length=3;config.upper=true;
  auto filtered=complete(config,folder/"filtered-reference.txt");budget_case(folder,"filtered",config,3,filtered);
  pg::Progress filteredRank;auto zero=snapshot(0);config.resource_percent=70;pg::run(config,(folder/"filtered-zero.txt").u8string(),false,filteredRank,{},&zero);
  check(filteredRank.scanned.load()==1&&filteredRank.written.load()==0&&filteredRank.disk_limit,"Rejected ranks before the first quota-blocked result are checkpointed");
  config.tokens={"a","b"};pg::Progress rejected;pg::run(config,(folder/"all-rejected.txt").u8string(),false,rejected,{},&zero);
  check(rejected.scanned.load()==pg::total(config)&&rejected.written.load()==0&&!rejected.disk_limit,"A zero disk quota still scans a completely rejected job to completion");

  config={};config.tokens={u8"آ",u8"😀"};config.max_length=2;
  auto unicode=complete(config,folder/"unicode-reference.txt");check(unicode==u8"آ\n😀\nآآ\nآ😀\n😀آ\n😀😀\n","Unicode oracle uses UTF-8 byte widths");budget_case(folder,"unicode",config,7,unicode);

  config={};config.tokens={"a","aa","b"};config.max_length=5;config.no_repeat=true;
  auto ambiguous=complete(config,folder/"ambiguous-reference.txt");unique_lines(ambiguous);budget_case(folder,"ambiguous",config,17,ambiguous);
  config.resource_percent=70;auto small=snapshot(17);auto repeated=folder/"repeated.txt";pg::Progress first;pg::run(config,repeated.u8string(),false,first,{},&small);
  auto previous=fs::file_size(repeated);write(repeated,"uncommitted crash tail\n",true);unsigned resumes=0;
  for(;resumes<200;++resumes){
   pg::Progress progress;pg::run({},repeated.u8string(),true,progress,{},&small);auto actual=read(repeated);
   check(actual.size()<=previous+17&&actual==ambiguous.substr(0,actual.size()),"Every small-quota resume commits only the next exact prefix");
   check(actual.size()>previous||progress.scanned.load()==progress.total.load(),"Adequate per-line quota makes forward progress");previous=actual.size();
   if(progress.scanned.load()==progress.total.load())break;
   check(progress.disk_limit,"Incomplete repeated resume reports the disk budget");
  }
  check(resumes<200&&read(repeated)==ambiguous,"Repeated tiny-quota resumes and crash-tail recovery match unlimited output");

  config={};config.tokens={"a","b"};config.min_length=config.max_length=19;config.threads=3;
  auto multi=complete(config,folder/"multi-reference.txt");
  // The first 4 MiB worker range fits, then thirteen lines fit in worker two.
  const uint64_t firstSpan=(4ull*1024*1024)/20;
  const uint64_t multiBudget=firstSpan*20+13*20+19;
  budget_case(folder,"multiworker",config,multiBudget,multi);

  for(unsigned version=1;version<=3;++version){
   auto output=folder/("legacy-"+std::to_string(version)+".txt");legacy(output,version);pg::Progress progress;pg::ResourceSnapshot unknown;bool manual=false;
   pg::run({},output.u8string(),true,progress,[&](const pg::Config& saved){manual=saved.resource_percent==0&&saved.threads==2;},&unknown);
   check(manual&&read(output)=="a\nb\naa\nab\nba\nbb\n","Legacy checkpoint remains unrestricted and ignores unknown resource detection");
   check(progress.disk_budget_bytes.load()==UINT64_MAX&&progress.active_workers.load()==2,"Legacy progress preserves its original worker policy");
   check(read(fs::u8path(output.u8string()+".pgstate")).find("PASSGEN_6\n")==0,"Legacy checkpoint upgrades to the new durable schema");
  }

  config={};config.tokens={"a","b"};config.max_length=2;config.resource_percent=70;
  auto rejectsBeforeCreate=[&](const std::string& label,const pg::ResourceSnapshot& resources,const pg::Config& settings){
   auto output=folder/(label+".txt");pg::Progress progress;bool rejected=false;try{pg::run(settings,output.u8string(),false,progress,{},&resources);}catch(const std::exception&){rejected=true;}
   check(rejected,"Unsafe resource snapshot is refused");check(!fs::exists(output)&&!fs::exists(fs::u8path(output.u8string()+".pgstate")),"Resource refusal happens before creating output or checkpoint");
  };
  auto low=snapshot();low.memory_available_bytes=1024*1024;rejectsBeforeCreate("low-memory",low,config);
  auto lineMemory=snapshot();lineMemory.memory_available_bytes=13ull*1024*1024;auto hugeLine=config;hugeLine.tokens={std::string(4096,'x')};hugeLine.max_length=128;rejectsBeforeCreate("line-buffer-memory",lineMemory,hugeLine);
  for(unsigned missing=0;missing<3;++missing){auto unknown=snapshot();if(missing==0)unknown.cpu_known=false;if(missing==1)unknown.memory_known=false;if(missing==2)unknown.disk_known=false;rejectsBeforeCreate("unknown-"+std::to_string(missing),unknown,config);}

  config={};config.tokens={"a","b"};config.token_limits={{"a",1}};config.max_length=3;config.compute_mode=pg::ComputeMode::GpuPreferred;
  const auto capped=complete(config,folder/"capped-reference.txt");check(capped=="a\nb\nab\nba\nbb\nabb\nbab\nbba\nbbb\n","Capped-token oracle includes zero uses and suppresses only excessive repetitions");
  budget_case(folder,"capped-partial",config,4,capped);
  config.tokens={"a","aa","b"};config.token_limits={{"a",1},{"aa",1}};config.max_length=4;
  const auto cappedAliases=complete(config,folder/"capped-alias-reference.txt");budget_case(folder,"capped-alias-partial",config,7,cappedAliases);

  // Measure real process CPU time; Windows std::clock measures wall time.
  config={};config.tokens={"a","b"};config.min_length=config.max_length=24;config.upper=true;config.resource_percent=25;
  auto one=snapshot();one.logical_cpus=1;one.cpu_capacity=1;pg::Progress paced;
  const auto wallStart=std::chrono::steady_clock::now();const double cpuStart=process_seconds();
  pg::run(config,(folder/"paced-cpu.txt").u8string(),false,paced,{},&one);
  const double usedCpu=process_seconds()-cpuStart;
  const double elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-wallStart).count();
  check(paced.scanned.load()==pg::total(config)&&paced.written.load()==0,"CPU-paced impossible filter completes all ranks");
  check(usedCpu>=.03,"CPU pacing workload is long enough for a meaningful measurement");
  check(usedCpu/elapsed<.6,"Twenty-five percent worker target substantially reduces measured CPU utilization");
  std::cout<<"CPU target=25% measured process CPU/wall="<<usedCpu/elapsed<<" cpu="<<usedCpu<<"s wall="<<elapsed<<"s\n";

  config.resource_percent=70;pg::Progress paced70;
  const auto wall70Start=std::chrono::steady_clock::now();const double cpu70Start=process_seconds();
  pg::run(config,(folder/"paced-cpu-70.txt").u8string(),false,paced70,{},&one);
  const double usedCpu70=process_seconds()-cpu70Start;
  const double elapsed70=std::chrono::duration<double>(std::chrono::steady_clock::now()-wall70Start).count();
  check(paced70.scanned.load()==pg::total(config)&&paced70.written.load()==0,"Seventy percent CPU-paced filter completes all ranks");
  check(usedCpu70>=.03&&usedCpu70/elapsed70<.9,"Seventy percent target remains below full measured CPU utilization");
  std::cout<<"CPU target=70% measured process CPU/wall="<<usedCpu70/elapsed70<<" cpu="<<usedCpu70<<"s wall="<<elapsed70<<"s\n";

  config.resource_percent=70;auto tiny=snapshot();tiny.logical_cpus=1;tiny.cpu_capacity=.02;auto stoppedOutput=folder/"stopped-throttle.txt";pg::Progress stopped;
  std::atomic<bool> loaded{false},done{false};
  std::thread stopper([&]{while(!loaded&&!done)std::this_thread::yield();if(done)return;std::this_thread::sleep_for(std::chrono::milliseconds(100));stopped.stop=true;});
  const auto stopStart=std::chrono::steady_clock::now();
  try{pg::run(config,stoppedOutput.u8string(),false,stopped,[&](const pg::Config&){loaded=true;},&tiny);}catch(...){done=true;stopper.join();throw;}done=true;stopper.join();
  const double stopElapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-stopStart).count();
  check(stopElapsed<2&&stopped.stop&&stopped.scanned.load()<stopped.total.load(),"Stop interrupts even a very small CPU allowance within two seconds");
  pg::Progress stopResume;auto ample=snapshot();pg::run({},stoppedOutput.u8string(),true,stopResume,{},&ample);
  check(stopResume.scanned.load()==stopResume.total.load()&&read(stoppedOutput).empty(),"CPU-throttled stop saves a resumable exact rank prefix");
  fs::remove_all(folder);std::cout<<"PASS: "<<checks<<" resource engine checks\n";return 0;
 }catch(const std::exception& error){std::cerr<<"FAIL after "<<checks<<" checks: "<<error.what()<<" | artifacts "<<folder<<"\n";return 1;}
}
