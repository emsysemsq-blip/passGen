#include "engine.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <chrono>
namespace fs=std::filesystem;
static void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
static std::string read(const fs::path& p){std::ifstream f(p,std::ios::binary);return {(std::istreambuf_iterator<char>(f)),{}};}
int main(){auto dir=fs::temp_directory_path()/("passgen-known-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));fs::create_directories(dir);try{
 for(bool direct:{false,true})for(bool chars:{false,true})for(auto compute:{pg::ComputeMode::Cpu,pg::ComputeMode::Automatic,pg::ComputeMode::GpuPreferred}){
  pg::Config c;c.direct_combinations=direct;c.character_lengths=chars;c.compute_mode=compute;c.prefix=u8"آ😀A1!";c.max_length=pg::default_max_length(c);c.lower=false;c.upper=c.digit=c.symbol=true;
  auto out=dir/(std::to_string(direct)+std::to_string(chars)+std::to_string(int(compute))+".txt");
  check(pg::total(c)==1,"Fixed-only count is one");check(pg::candidate(c,0)==c.prefix,"Fixed-only candidate");check(pg::preview(c).samples==std::vector<std::string>{c.prefix},"Fixed-only preview");
  pg::Progress stopped;pg::run(c,out.u8string(),false,stopped,[&](const pg::Config&){stopped.stop=true;});check(read(out).empty()&&stopped.scanned.load()==0,"Stop before single result");
  {std::ofstream f(out,std::ios::app|std::ios::binary);f<<"crash tail";}
  pg::Progress p;pg::run({},out.u8string(),true,p);check(read(out)==c.prefix+"\n"&&p.written==1&&p.scanned.load()==1&&!p.gpu_used,"Fixed-only exact crash recovery and CPU fallback");
  pg::Progress again;pg::run({},out.u8string(),true,again);check(read(out)==c.prefix+"\n"&&again.written==1,"Idempotent fixed-only Resume");
  c.lower=true;check(pg::preview(c).samples.empty(),"Fixed-only active rule enforced");auto rejected=out;rejected+=".rule";pg::Progress r;pg::run(c,rejected.u8string(),false,r);check(read(rejected).empty()&&r.scanned.load()==1,"Unsatisfied fixed rule emits nothing");
  c.lower=false;c.min_length=c.max_length=6;check(pg::total(c)==0,"Fixed length outside interval");pg::Progress z;auto empty=out;empty+=".length";pg::run(c,empty.u8string(),false,z);check(read(empty).empty()&&z.total.load()==0,"Zero-length-space job is safe");
 }
 pg::Config c;c.character_lengths=true;c.direct_combinations=true;c.tokens={"a","b"};c.min_length=c.max_length=4;c.prefix="Am1";
 for(unsigned mode=0;mode<3;++mode){if(mode==0){c.prefix="";c.suffix="Am1";}if(mode==1){c.prefix="Am1";c.suffix="";}if(mode==2){c.prefix="Am";c.suffix="1";}
  c.lower=c.upper=c.digit=true;auto out=dir/("mode"+std::to_string(mode));pg::Progress p;pg::run(c,out.u8string(),false,p);
  std::string expected=mode==0?"aAm1\nbAm1\n":mode==1?"Am1a\nAm1b\n":"Ama1\nAmb1\n";check(read(out)==expected,"Placement order and full-line rules");
 }
 c={};c.character_lengths=true;c.prefix="A";c.suffix="1";c.max_length=pg::default_max_length(c);check(pg::preview(c).samples==std::vector<std::string>{"A1"},"Two fixed parts concatenate once");
 c={};bool threw=false;try{pg::total(c);}catch(const std::exception&){threw=true;}check(threw,"All-empty inputs still rejected");
 fs::remove_all(dir);std::cout<<"PASS: known parts, fixed-only modes, Unicode, class/length rules, Stop/crash/Resume\n";return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<"; files: "<<dir<<"\n";return 1;}}
