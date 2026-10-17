#include "engine.hpp"
#include <iostream>
#include <thread>
#include <csignal>
#include <chrono>
static volatile std::sig_atomic_t interrupted=0;
static void signal_stop(int){interrupted=1;}
int pg_main(int argc,char** argv){try{pg::Config c;c.character_lengths=true;c.threads=std::min(8u,std::max(1u,std::thread::hardware_concurrency()));std::string input,output,symbols,emojis;bool words=false,resume=false,hasSymbols=false,persian=false,shift=false;bool haveMax=false;
 for(int i=1;i<argc;++i){std::string a=argv[i];auto value=[&](){if(++i>=argc)throw std::runtime_error("Missing option value");return std::string(argv[i]);};auto num=[&](){auto s=value();if(s.empty()||s.find_first_not_of("0123456789")!=std::string::npos)throw std::runtime_error("Invalid numeric option");auto n=std::stoul(s);if(n>128)throw std::runtime_error("Numeric option too large");return(unsigned)n;};
 if(a=="--emojis")emojis=value();else if(a=="--persian")persian=true;else if(a=="--shift-letters")shift=true;else if(a=="--token-lengths")c.character_lengths=false;else if(a=="--chars")input=value();else if(a=="--words"){input=value();words=true;}else if(a=="--out")output=value();else if(a=="--min")c.min_length=num();else if(a=="--max"){c.max_length=num();haveMax=true;}else if(a=="--threads")c.threads=num();else if(a=="--prefix")c.prefix=value();else if(a=="--suffix")c.suffix=value();else if(a=="--lower")c.lower=true;else if(a=="--upper")c.upper=true;else if(a=="--digit")c.digit=true;else if(a=="--symbols"){symbols=value();hasSymbols=true;}else if(a=="--symbol")c.symbol=true;else if(a=="--no-repeat")c.no_repeat=true;else if(a=="--resume")resume=true;else if(a=="--help"){std::cout<<"PassGen 0.1.5\n--chars TEXT | --words 'WORD WORD' --out FILE\n[--min N --max N --threads N --prefix TEXT --suffix TEXT]\n[--lower --upper --digit --symbols TEXT --emojis 'EMOJI EMOJI' --persian --shift-letters --no-repeat --token-lengths]\n--resume --out FILE restores settings from FILE.pgstate\nBlank lengths: 1 through an input-based default (up to 128). Default units: final Unicode characters. --token-lengths selects legacy token counts. Duplicate output lines are always excluded.\n";return 0;}else throw std::runtime_error("Unknown option: "+a);}
 if(output.empty())throw std::runtime_error("--out FILE is required");
 if(!resume){c.tokens=pg::tokenize(input,words);if(hasSymbols)pg::add_symbols(c,symbols);pg::add_emojis(c,emojis);if(persian)pg::add_persian(c);if(shift)pg::add_shift_letters(c);if(!haveMax)c.max_length=pg::default_max_length(c);pg::total(c);}
 pg::Progress p;std::atomic<bool> done{false};std::signal(SIGINT,signal_stop);std::thread watcher([&]{while(!done){if(interrupted)p.stop=true;std::this_thread::sleep_for(std::chrono::milliseconds(50));}});auto began=std::chrono::steady_clock::now();try{pg::run(c,output,resume,p);}catch(...){done=true;watcher.join();throw;}done=true;watcher.join();std::cout<<(p.scanned.load()==p.total.load()?"Complete":"Stopped")<<": scanned="<<p.scanned.load()<<" written="<<p.written<<" bytes="<<p.bytes<<" seconds="<<std::chrono::duration<double>(std::chrono::steady_clock::now()-began).count()<<"\n";return p.scanned.load()==p.total.load()?0:2;
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
