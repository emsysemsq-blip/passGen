#include "engine.hpp"
#include "gpu.hpp"
#include "version.hpp"
#include <iostream>
#include <thread>
#include <csignal>
#include <chrono>
#include <condition_variable>
#include <mutex>
static volatile std::sig_atomic_t interrupted=0;
static void signal_stop(int){interrupted=1;}
int pg_main(int argc,char** argv){try{pg::Config c;c.compute_mode=pg::ComputeMode::Automatic;c.resource_percent=70;c.character_lengths=true;c.threads=std::min(8u,std::max(1u,std::thread::hardware_concurrency()));std::string input,output,symbols,emojis;bool words=false,resume=false,hasSymbols=false,persian=false,shift=false;bool haveMax=false,manualThreads=false,explicitResources=false,listGpus=false;
 for(int i=1;i<argc;++i){std::string a=argv[i];auto value=[&](){if(++i>=argc)throw std::runtime_error("Missing option value");return std::string(argv[i]);};auto num=[&](){auto s=value();if(s.empty()||s.find_first_not_of("0123456789")!=std::string::npos)throw std::runtime_error("Invalid numeric option");auto n=std::stoul(s);if(n>128)throw std::runtime_error("Numeric option too large");return(unsigned)n;};
 if(a=="--limit"){auto token=value();auto cap=num();if(cap<1||cap>10)throw std::runtime_error("--limit TOKEN N requires N=1..10");c.token_limits[token]=cap;}else if(a=="--compute"){auto mode=value();if(mode=="auto")c.compute_mode=pg::ComputeMode::Automatic;else if(mode=="cpu")c.compute_mode=pg::ComputeMode::Cpu;else if(mode=="gpu")c.compute_mode=pg::ComputeMode::GpuPreferred;else throw std::runtime_error("--compute must be auto, cpu, or gpu");}else if(a=="--list-gpus")listGpus=true;else if(a=="--emojis")emojis=value();else if(a=="--persian")persian=true;else if(a=="--shift-letters")shift=true;else if(a=="--token-lengths")c.character_lengths=false;else if(a=="--chars"){input=value();words=false;c.direct_combinations=false;}else if(a=="--words"){input=value();words=true;c.direct_combinations=true;}else if(a=="--out")output=value();else if(a=="--min")c.min_length=num();else if(a=="--max"){c.max_length=num();haveMax=true;}else if(a=="--threads"){c.threads=num();manualThreads=true;}else if(a=="--resource-percent"){c.resource_percent=num();explicitResources=true;}else if(a=="--prefix")c.prefix=value();else if(a=="--suffix")c.suffix=value();else if(a=="--lower")c.lower=true;else if(a=="--upper")c.upper=true;else if(a=="--digit")c.digit=true;else if(a=="--symbols"){symbols=value();hasSymbols=true;}else if(a=="--symbol")c.symbol=true;else if(a=="--no-repeat")c.no_repeat=true;else if(a=="--resume")resume=true;else if(a=="--help"){std::cout<<"PassGen " PASSGEN_VERSION "\n--chars TEXT | --words 'WORD WORD' --out FILE\n[--min N --max N --threads N --resource-percent 0..100 --compute auto|cpu|gpu --prefix TEXT --suffix TEXT]\n[--limit TOKEN 1..10 (repeatable); omitted tokens are unlimited]\n[--lower --upper --digit --symbols TEXT --emojis 'EMOJI EMOJI' --persian --shift-letters --no-repeat --token-lengths]\n--words uses whole-word CPU traversal with prefix pruning; constraints are checked before extending a branch.\nDefault: automatic 70% generation CPU target and RAM-buffer/free-disk budgets. --threads selects manual mode unless --resource-percent is explicit. 0 disables budgets. Compute mode defaults to auto: compatible OpenCL GPU plus CPU, with CPU fallback. --compute gpu prefers the GPU with CPU fallback; --compute cpu disables GPU generation.\n--list-gpus reports OpenCL GPU names, memory and discovery status without an output file.\n--resume --out FILE restores settings from FILE.pgstate\nBlank lengths: 1 through an input-based default (up to 128). Default units: final Unicode characters. --token-lengths selects legacy token counts. Duplicate output lines are always excluded. --shift-letters adds Windows Persian Shift variants, not English uppercase.\n";return 0;}else throw std::runtime_error("Unknown option: "+a);}
 if(manualThreads&&!explicitResources)c.resource_percent=0;
 if(listGpus){auto inventory=pg::query_gpu();std::cout<<"GPU discovery: "<<inventory.status<<"\n";for(const auto& device:inventory.devices){std::cout<<"GPU: "<<device.name<<" | vendor="<<device.vendor<<" | driver="<<device.driver<<" | memory="<<device.global_memory<<" bytes | max allocation="<<device.max_allocation<<" bytes | "<<(device.unified_memory?"shared memory":"dedicated memory")<<" | "<<(device.available?"available":"unavailable")<<"\n";}return 0;}
 if(output.empty())throw std::runtime_error("--out FILE is required");
 if(!resume){c.tokens=pg::tokenize(input,words);if(hasSymbols)pg::add_symbols(c,symbols);pg::add_emojis(c,emojis);if(persian)pg::add_persian(c);if(shift)pg::add_shift_letters(c);if(!haveMax)c.max_length=pg::default_max_length(c);pg::total(c);}
 pg::Progress p;std::atomic<bool> done{false};std::signal(SIGINT,signal_stop);std::mutex waitMutex;std::condition_variable wake;std::thread watcher([&]{std::unique_lock<std::mutex> lock(waitMutex);while(!done){if(interrupted)p.stop=true;wake.wait_for(lock,std::chrono::milliseconds(50),[&]{return done.load();});}});auto began=std::chrono::steady_clock::now();try{pg::run(c,output,resume,p,[&](const pg::Config& saved){std::cerr<<"Resources: "<<(saved.resource_percent?std::to_string(saved.resource_percent)+"% auto":"manual")<<" | workers="<<p.active_workers.load()<<" | buffer/worker="<<p.buffer_bytes.load()<<" bytes | disk budget=";if(saved.resource_percent)std::cerr<<p.disk_budget_bytes.load();else std::cerr<<"unlimited";std::cerr<<" | Compute: "<<p.compute_status.get()<<"\n";});}catch(...){{std::lock_guard<std::mutex> lock(waitMutex);done=true;}wake.notify_one();watcher.join();throw;}{std::lock_guard<std::mutex> lock(waitMutex);done=true;}wake.notify_one();watcher.join();std::cerr<<"Compute result: "<<p.compute_status.get()<<" | GPU scanned="<<p.gpu_scanned.load()<<"\n";if(p.direct_combinations)std::cerr<<"Constructed combinations="<<p.constructed.load()<<"; covered raw ranks include skipped branches\n";if(p.disk_limit)std::cerr<<"Stopped safely at the free-disk budget. Free space or choose Resume to calculate a fresh budget.\n";std::cout<<(p.scanned.load()==p.total.load()?"Complete":"Stopped")<<": scanned="<<p.scanned.load()<<" written="<<p.written<<" bytes="<<p.bytes<<" seconds="<<std::chrono::duration<double>(std::chrono::steady_clock::now()-began).count()<<"\n";return p.scanned.load()==p.total.load()?0:2;
 }catch(const std::exception& e){std::cerr<<"PassGen: "<<e.what()<<"\n";return 1;}}

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
int wmain(int argc,wchar_t** argv){
 SetConsoleOutputCP(CP_UTF8);
 std::vector<std::string> storage;
 for(int i=0;i<argc;++i){int n=WideCharToMultiByte(CP_UTF8,0,argv[i],-1,nullptr,0,nullptr,nullptr);std::string s(n,0);WideCharToMultiByte(CP_UTF8,0,argv[i],-1,s.data(),n,nullptr,nullptr);s.pop_back();storage.push_back(std::move(s));}
 std::vector<char*> args;for(auto& s:storage)args.push_back(s.data());return pg_main(argc,args.data());
}
#else
int main(int argc,char** argv){return pg_main(argc,argv);}
#endif
