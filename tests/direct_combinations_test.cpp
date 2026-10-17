#include "engine.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
#include <sstream>
#include <stdexcept>
#include <chrono>
namespace fs=std::filesystem;
static std::string read(const fs::path& p){std::ifstream f(p,std::ios::binary);return {(std::istreambuf_iterator<char>(f)),{}};}
static void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(){
 const auto dir=fs::temp_directory_path()/("passgen-direct-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));fs::create_directories(dir);
 try{
  pg::Config c;c.direct_combinations=true;c.tokens={"a","b"};c.token_limits={{"a",1},{"b",1}};c.max_length=128;c.threads=3;
  pg::Progress p;pg::run(c,(dir/"huge.txt").u8string(),false,p);
  check(read(dir/"huge.txt")=="a\nb\nab\nba\n","Exact four feasible combinations across lengths 1..128");
  check(p.constructed==4&&p.scanned.load()==pg::total(c),"Only four complete combinations built while impossible >64-bit branches are skipped");
  c.min_length=128;pg::Progress impossible;pg::run(c,(dir/"impossible.txt").u8string(),false,impossible);
  check(impossible.constructed==0&&read(dir/"impossible.txt").empty()&&impossible.scanned.load()==pg::total(c),"Impossible capped tree produces no complete combinations");
  c.token_limits.clear();c.upper=true;pg::Progress classes;pg::run(c,(dir/"classes.txt").u8string(),false,classes);
  check(classes.constructed==0&&classes.scanned.load()==pg::total(c),"Unavailable required class prunes entire huge tree");
  c={};c.direct_combinations=true;c.tokens={"a","b"};c.token_limits={{"a",1}};c.min_length=c.max_length=24;c.threads=3;
  pg::Progress stopped;std::atomic<bool> done{false};std::thread stopper([&]{while(!done&&stopped.scanned.load()==pg::Index(0))std::this_thread::yield();if(!done)stopped.stop=true;});
  try{pg::run(c,(dir/"stopped.txt").u8string(),false,stopped);}catch(...){done=true;stopper.join();throw;}done=true;stopper.join();
  check(stopped.scanned.load()>pg::Index(0)&&stopped.scanned.load()<stopped.total.load(),"Direct multiworker stop preserves a nonempty partial cursor");
  auto prefix=read(dir/"stopped.txt");{std::ofstream f(dir/"stopped.txt",std::ios::binary|std::ios::app);f<<"crash tail";}
  pg::Progress resumed;bool policy=false;pg::run({},(dir/"stopped.txt").u8string(),true,resumed,[&](const pg::Config& active){policy=active.direct_combinations&&active.token_limits==c.token_limits;});
  std::string expected;for(unsigned pos=0;pos<24;++pos)expected+=std::string(pos,'b')+'a'+std::string(23-pos,'b')+'\n';expected+=std::string(24,'b')+'\n';
  check(policy&&read(dir/"stopped.txt")==expected&&expected.compare(0,prefix.size(),prefix)==0,"Direct cursor and limits survive Stop, crash tail and Resume with exact ordered bytes");
  check(resumed.scanned.load()==resumed.total.load()&&resumed.written==25,"Pruned gaps do not lose or duplicate resumed combinations");
  auto examples=pg::preview(c);check(!examples.samples.empty(),"Direct preview finds feasible constrained words");
  c={};c.direct_combinations=true;c.tokens={"a","b"};c.lower=true;c.min_length=c.max_length=18;c.threads=4;
  pg::Progress flat;pg::run(c,(dir/"flat.txt").u8string(),false,flat);c.direct_combinations=false;pg::Progress reference;pg::run(c,(dir/"flat-reference.txt").u8string(),false,reference);
  check(read(dir/"flat.txt")==read(dir/"flat-reference.txt")&&flat.constructed==262144&&flat.written==262144,"Proven legal direct jobs use the fast renderer with exact bytes and honest counters");
  auto hash=[](const std::string& text){uint64_t value=14695981039346656037ull;for(unsigned char byte:text){value^=byte;value*=1099511628211ull;}return value;};
  pg::Index huge=1;for(unsigned n=0;n<128;++n)huge*=2;const auto tail=dir/"wide-tail.txt";{std::ofstream output(tail,std::ios::binary);}
  std::ostringstream body;body<<"PASSGEN_7\n"<<(huge-pg::Index(257))<<" 0 0 "<<hash("")<<"\n128 128 3 0 0 0 0 0 0 0 0 1\n- -\n2\n61\n62\n1\n61 1\n";auto content=body.str();{std::ofstream state(fs::u8path(tail.u8string()+".pgstate"),std::ios::binary);state<<content<<"CHECK "<<hash(content)<<'\n';}
  pg::Progress wide;pg::run({},tail.u8string(),true,wide);std::string wideExpected;for(unsigned pos=119;pos<128;++pos)wideExpected+=std::string(pos,'b')+'a'+std::string(127-pos,'b')+'\n';wideExpected+=std::string(128,'b')+'\n';
  check(read(tail)==wideExpected&&wide.constructed==10&&wide.scanned.load()==huge,"Narrow local traversal preserves a 128-bit resumed cursor and constrained prefix counts");
  c={};c.direct_combinations=true;c.character_lengths=true;c.tokens={"a",std::string(4096,'X')};c.min_length=c.max_length=128;c.resource_percent=70;
  pg::ResourceSnapshot small;small.logical_cpus=4;small.cpu_capacity=4;small.cpu_known=true;small.memory_known=true;small.memory_available_bytes=13ull*1024*1024;small.disk_known=true;small.disk_available_bytes=16ull*1024*1024;
  pg::Progress bounded;pg::run(c,(dir/"scalar-buffer.txt").u8string(),false,bounded,{},&small);
  check(read(dir/"scalar-buffer.txt")==std::string(128,'a')+'\n'&&bounded.constructed==1,"Final scalar length bounds buffers without multiplying by unusably long word size");
  check(bounded.buffer_bytes>=129&&bounded.buffer_bytes<524289,"A constrained low-RAM plan fits the actual maximum line safely");
  fs::remove_all(dir);std::cout<<"PASS: direct prefix pruning, >64-bit skipped branches, no invalid constructed leaves, exact multiworker stop/crash/resume\n";return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<" artifacts "<<dir<<'\n';return 1;}
}
