#include "engine.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <thread>
#include <stdexcept>
#include <chrono>
namespace fs=std::filesystem;
static int checks=0;
static void check(bool v,const char* msg){++checks;if(!v)throw std::runtime_error(msg);}
template<class F>static void rejects(F f,const char* msg){bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}check(rejected,msg);}
static std::string read(const fs::path& p){std::ifstream f(p,std::ios::binary);return std::string((std::istreambuf_iterator<char>(f)),{});}
static void append(const fs::path& p,const std::string& text){std::ofstream f(p,std::ios::app|std::ios::binary);f<<text;}
int main(){fs::path dir=fs::temp_directory_path()/("passgen-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));fs::create_directories(dir);try{
 pg::Config c;c.tokens=pg::tokenize("aab",false);c.max_length=2;
 check(c.tokens.size()==2,"deduplicate characters");check(pg::total(c)==6,"complete count");check(pg::candidate(c,0)=="a"&&pg::candidate(c,5)=="bb","index boundaries");
 pg::Progress p;pg::run(c,(dir/"one.txt").u8string(),false,p);check(read(dir/"one.txt")=="a\nb\naa\nab\nba\nbb\n","exact order, no conditions");check(p.scanned==6&&p.written==6&&p.bytes==16,"progress counters");
 c.threads=4;pg::Progress parallel;pg::run(c,(dir/"parallel.txt").u8string(),false,parallel);check(read(dir/"parallel.txt")==read(dir/"one.txt"),"parallel determinism");
 c.no_repeat=true;pg::Progress nr;pg::run(c,(dir/"nr.txt").u8string(),false,nr);check(read(dir/"nr.txt")=="a\nb\nab\nba\n","adjacent duplicate filter");
 auto tokens=pg::tokenize(u8"آبآ",false);check(tokens.size()==2&&tokens[0]==u8"آ","Unicode codepoints");c={};c.tokens=tokens;c.max_length=2;pg::Progress unicode;pg::run(c,(dir/fs::u8path(u8"فارسی.txt")).u8string(),false,unicode);check(unicode.written==6,"Unicode output path and count");check(read(dir/fs::u8path(u8"فارسی.txt")).find(u8"آب\n")!=std::string::npos,"Unicode byte preservation");
 c={};c.tokens=pg::tokenize("red blue red",true);c.max_length=2;check(pg::total(c)==6&&pg::candidate(c,3)=="redblue","word concatenation");
 c={};c.tokens={"a","A","1","!"};c.min_length=4;c.max_length=4;c.lower=c.upper=c.digit=c.symbol=true;pg::Progress filtered;pg::run(c,(dir/"filtered.txt").u8string(),false,filtered);check(filtered.written==24,"all class requirements");
 c={};c.tokens={"b"};c.lower=c.upper=c.digit=c.symbol=true;c.prefix="A1!";check(pg::accepts(c,pg::candidate(c,0)),"prefix participates in rules");
 c={};c.tokens={"a","b"};c.max_length=2;pg::Progress stopped;pg::run(c,(dir/"resume.txt").u8string(),false,stopped,[&](const pg::Config&){stopped.stop=true;});check(stopped.scanned==0&&fs::exists(dir/"resume.txt.pgstate"),"stop creates durable initial checkpoint");
 append(dir/"resume.txt","uncommitted partial data");pg::Progress resumed;pg::run({},(dir/"resume.txt").u8string(),true,resumed);check(read(dir/"resume.txt")==read(dir/"one.txt"),"resume removes uncommitted tail and restores config");
 pg::Progress completed;pg::run({},(dir/"resume.txt").u8string(),true,completed);check(read(dir/"resume.txt")==read(dir/"one.txt")&&completed.written==6,"completed resume is idempotent");
 rejects([&]{pg::Progress q;pg::run(c,(dir/"one.txt").u8string(),false,q);},"never overwrite existing output");
 append(dir/"resume.txt.pgstate","CHECK 1\n");rejects([&]{pg::Progress q;pg::run({},(dir/"resume.txt").u8string(),true,q);},"corrupt checkpoint rejected");
 {std::fstream f(dir/"one.txt",std::ios::in|std::ios::out|std::ios::binary);f.put('x');}rejects([&]{pg::Progress q;pg::run({},(dir/"one.txt").u8string(),true,q);},"changed tail rejected");
 fs::resize_file(dir/"parallel.txt",0);rejects([&]{pg::Progress q;pg::run({},(dir/"parallel.txt").u8string(),true,q);},"truncated output rejected");
 c.max_length=64;rejects([&]{pg::total(c);},"index overflow rejected");c.tokens.clear();rejects([&]{pg::total(c);},"empty input rejected");
 rejects([]{pg::tokenize(std::string("\xc0\x80",2),false);},"invalid UTF8 rejected");rejects([]{pg::tokenize(std::string("\xed\xa0\x80",3),false);},"surrogate rejected");rejects([]{pg::tokenize("a\nb",false);},"embedded newlines rejected");
 c={};c.tokens={"a","b"};c.max_length=19;c.threads=2;pg::Progress middle;std::atomic<bool> quit{false};std::thread stopper([&]{while(!quit&&!middle.scanned)std::this_thread::yield();if(!quit)middle.stop=true;});try{pg::run(c,(dir/"middle.txt").u8string(),false,middle);}catch(...){quit=true;stopper.join();throw;}quit=true;stopper.join();check(middle.scanned>0&&middle.scanned<pg::total(c),"stop after committed nonempty batch");auto partial=read(dir/"middle.txt");append(dir/"middle.txt","simulated crash tail\n");pg::Progress recovery;pg::run({},(dir/"middle.txt").u8string(),true,recovery);check(recovery.scanned==pg::total(c)&&recovery.written==pg::total(c),"nonempty checkpoint resumed completely");auto actual=read(dir/"middle.txt");check(actual.compare(0,partial.size(),partial)==0,"committed prefix retained");std::string expected;for(uint64_t i=0;i<pg::total(c);++i){expected+=pg::candidate(c,i);expected+='\n';}check(actual==expected,"resumed output exactly matches full indexed sequence");
 fs::remove_all(dir);std::cout<<"PASS: "<<checks<<" checks\n";return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<" checks: "<<e.what()<<" | artifacts "<<dir<<"\n";return 1;}}
