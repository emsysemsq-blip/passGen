#include "engine.hpp"
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <future>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <unordered_set>
#include <cstring>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#include <sys/file.h>
#endif
namespace pg {
namespace fs=std::filesystem;
static void require(bool b,const char* s){if(!b)throw std::runtime_error(s);}
static fs::path path(const std::string& s){return fs::u8path(s);}
std::vector<std::string> tokenize(const std::string& text,bool words){
 std::vector<std::string> out; std::unordered_set<std::string> seen;
 auto add=[&](std::string s){if(!s.empty() && seen.insert(s).second)out.push_back(std::move(s));};
 if(words){std::istringstream in(text);std::string s;while(in>>s)add(s);}
 else for(size_t i=0;i<text.size();){
  unsigned char c=text[i];size_t n=c<0x80?1:(c>=0xC2&&c<=0xDF?2:(c>=0xE0&&c<=0xEF?3:(c>=0xF0&&c<=0xF4?4:0)));
  require(n && i+n<=text.size(),"Invalid UTF-8 input");
  for(size_t j=1;j<n;++j)require((static_cast<unsigned char>(text[i+j])&0xC0)==0x80,"Invalid UTF-8 input");
  if(n==3)require(!((c==0xE0&&(unsigned char)text[i+1]<0xA0)||(c==0xED&&(unsigned char)text[i+1]>=0xA0)),"Invalid Unicode scalar");
  if(n==4)require(!((c==0xF0&&(unsigned char)text[i+1]<0x90)||(c==0xF4&&(unsigned char)text[i+1]>=0x90)),"Invalid Unicode scalar");
  require(c!='\n'&&c!='\r'&&c!=0,"Input cannot contain newline or NUL");add(text.substr(i,n));i+=n;
 }
 return out;
}
uint64_t total(const Config& c){
 require(!c.tokens.empty(),"Enter at least one character or word");
 require(c.min_length>=1 && c.max_length>=c.min_length && c.max_length<=128,"Length must be 1..128 and minimum <= maximum");
 require(c.threads>=1 && c.threads<=32,"Worker count must be 1..32");
 size_t longest=0;for(auto& s:c.tokens){require(!s.empty()&&s.find_first_of("\r\n") == std::string::npos && s.find('\0')==std::string::npos,"Invalid token");longest=std::max(longest,s.size());}
 require(longest<=4096 && c.prefix.size()<=4096 && c.suffix.size()<=4096,"Token, prefix or suffix too long");
 require(c.prefix.find_first_of("\r\n")==std::string::npos && c.suffix.find_first_of("\r\n")==std::string::npos,"Prefix/suffix cannot contain newlines");
 uint64_t p=1,sum=0,n=c.tokens.size();
 for(unsigned l=1;l<=c.max_length;++l){require(p<=UINT64_MAX/n,"Search space exceeds 64-bit indexing; reduce length or input");p*=n;
  if(l>=c.min_length){require(sum<=UINT64_MAX-p,"Search space exceeds 64-bit indexing; reduce length or input");sum+=p;}}
 return sum;
}
std::string candidate(const Config& c,uint64_t index){
 uint64_t power=1;unsigned len=0;for(unsigned l=1;l<=c.max_length;++l){power*=c.tokens.size();if(l>=c.min_length){if(index<power){len=l;break;}index-=power;}}
 require(len!=0,"Candidate index out of range");
 std::array<size_t,128> digits{};for(unsigned i=len;i>0;--i){digits[i-1]=index%c.tokens.size();index/=c.tokens.size();}
 if(c.no_repeat)for(unsigned i=1;i<len;++i)if(digits[i]==digits[i-1])return {};
 std::string s=c.prefix;for(unsigned i=0;i<len;++i)s+=c.tokens[digits[i]];s+=c.suffix;return s;
}
bool accepts(const Config& c,const std::string& s){
 if(s.empty())return false;
 bool lo=false,up=false,di=false,sy=false;
 for(unsigned char v:s){lo|=v>='a'&&v<='z';up|=v>='A'&&v<='Z';di|=v>='0'&&v<='9';sy|=v>=33&&v<=126&&!((v>='a'&&v<='z')||(v>='A'&&v<='Z')||(v>='0'&&v<='9'));}
 return (!c.lower||lo)&&(!c.upper||up)&&(!c.digit||di)&&(!c.symbol||sy);
}
class File {
#ifdef _WIN32
 HANDLE h=INVALID_HANDLE_VALUE;
#else
 int h=-1;
#endif
public:
 File(const fs::path& p,bool existing){
#ifdef _WIN32
 h=CreateFileW(p.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,existing?OPEN_EXISTING:CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);require(h!=INVALID_HANDLE_VALUE,"Cannot open output (exists, locked, or inaccessible)");
#else
 h=::open(p.c_str(),O_RDWR|(existing?0:O_CREAT|O_EXCL),0600);require(h>=0,"Cannot open output (exists or inaccessible)");if(flock(h,LOCK_EX|LOCK_NB)!=0){close(h);h=-1;throw std::runtime_error("Output is locked by another job");}
#endif
 }
 ~File(){
#ifdef _WIN32
 if(h!=INVALID_HANDLE_VALUE)CloseHandle(h);
#else
 if(h>=0)close(h);
#endif
 }
 uint64_t size(){
#ifdef _WIN32
 LARGE_INTEGER n;require(GetFileSizeEx(h,&n),"Cannot read output size");return n.QuadPart;
#else
 auto n=lseek(h,0,SEEK_END);require(n>=0,"Cannot read output size");return n;
#endif
 }
 void seek(uint64_t n){
#ifdef _WIN32
 LARGE_INTEGER pos;pos.QuadPart=n;require(SetFilePointerEx(h,pos,nullptr,FILE_BEGIN),"Seek failed");
#else
 require(lseek(h,n,SEEK_SET)>=0,"Seek failed");
#endif
 }
 void truncate(uint64_t n){seek(n);
#ifdef _WIN32
 require(SetEndOfFile(h),"Truncate failed");
#else
 require(ftruncate(h,n)==0,"Truncate failed");
#endif
 }
 void write(const std::string& s){size_t i=0;while(i<s.size()){
#ifdef _WIN32
 DWORD n=0;require(WriteFile(h,s.data()+i,(DWORD)std::min<size_t>(s.size()-i,1<<24),&n,nullptr)&&n,"Write failed (check free disk space)");
#else
 auto n=::write(h,s.data()+i,s.size()-i);require(n>0,"Write failed (check free disk space)");
#endif
 i+=n;}}
 std::string tail(uint64_t end){size_t n=std::min<uint64_t>(end,65536);seek(end-n);std::string s(n,'\0');size_t i=0;while(i<n){
#ifdef _WIN32
 DWORD got=0;require(ReadFile(h,&s[i],(DWORD)(n-i),&got,nullptr)&&got,"Read failed");
#else
 auto got=::read(h,&s[i],n-i);require(got>0,"Read failed");
#endif
 i+=got;}seek(end);return s;}
 void flush(){
#ifdef _WIN32
 require(FlushFileBuffers(h),"Output flush failed");
#else
 require(fsync(h)==0,"Output flush failed");
#endif
 }
};
static uint64_t hash(const std::string& s){uint64_t v=14695981039346656037ull;for(unsigned char b:s){v^=b;v*=1099511628211ull;}return v;}
static void replace(const fs::path& a,const fs::path& b){
#ifdef _WIN32
 require(MoveFileExW(a.c_str(),b.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH),"Checkpoint replace failed");
#else
 fs::rename(a,b);auto parent=b.parent_path();int d=open(parent.empty()?".":parent.c_str(),O_RDONLY|O_DIRECTORY);require(d>=0,"Checkpoint directory open failed");int result=fsync(d);close(d);require(result==0,"Checkpoint directory flush failed");
#endif
}
static std::string encode(const std::string& s){static const char h[]="0123456789abcdef";std::string out;for(unsigned char b:s){out+=h[b>>4];out+=h[b&15];}return out.empty()?"-":out;}
static std::string decode(const std::string& s){if(s=="-")return {};require(s.size()%2==0,"Invalid checkpoint");std::string out;auto val=[](char c){if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;throw std::runtime_error("Invalid checkpoint");};for(size_t i=0;i<s.size();i+=2)out+=char(val(s[i])*16+val(s[i+1]));return out;}
struct State{Config c;uint64_t next=0,lines=0,offset=0,tailhash=hash("");};
static std::string serialize(const State& s){std::ostringstream o;o<<"PASSGEN_1\n"<<s.next<<' '<<s.lines<<' '<<s.offset<<' '<<s.tailhash<<'\n';auto& c=s.c;o<<c.min_length<<' '<<c.max_length<<' '<<c.threads<<' '<<c.lower<<' '<<c.upper<<' '<<c.digit<<' '<<c.symbol<<' '<<c.no_repeat<<'\n'<<encode(c.prefix)<<' '<<encode(c.suffix)<<'\n'<<c.tokens.size()<<'\n';for(auto& t:c.tokens)o<<encode(t)<<'\n';return o.str();}
static void save(const fs::path& p,const State& s){auto tmp=p;tmp+=".tmp";std::error_code ec;fs::remove(tmp,ec);{File f(tmp,false);auto body=serialize(s);f.write(body+"CHECK "+std::to_string(hash(body))+"\n");f.flush();}replace(tmp,p);}
static State load(const fs::path& p){require(fs::file_size(p)<=16*1024*1024,"Checkpoint too large");std::ifstream f(p,std::ios::binary);std::string all((std::istreambuf_iterator<char>(f)),{});auto at=all.rfind("CHECK ");require(at!=std::string::npos,"Invalid checkpoint");auto body=all.substr(0,at);std::istringstream check(all.substr(at+6));uint64_t checksum=0;require(bool(check>>checksum)&&checksum==hash(body),"Checkpoint checksum mismatch");std::istringstream in(body);std::string magic,a,b;State s;size_t n=0;auto& c=s.c;require(bool(in>>magic>>s.next>>s.lines>>s.offset>>s.tailhash>>c.min_length>>c.max_length>>c.threads>>c.lower>>c.upper>>c.digit>>c.symbol>>c.no_repeat>>a>>b>>n)&&magic=="PASSGEN_1"&&n<=100000,"Invalid checkpoint");c.prefix=decode(a);c.suffix=decode(b);for(size_t i=0;i<n;++i){require(bool(in>>a),"Invalid checkpoint");c.tokens.push_back(decode(a));}auto count=total(c);require(s.next<=count&&s.lines<=s.next&&s.offset<=INT64_MAX,"Invalid checkpoint position");return s;}
void run(Config c,const std::string& output,bool resume,Progress& progress,const std::function<void(const Config&)>& loaded){
 auto out=fs::absolute(path(output));auto cp=out;cp+=".pgstate";State s;
 if(resume){s=load(cp);c=s.c;}else {total(c);require(!fs::exists(cp),"Checkpoint already exists; select another output or resume");s.c=c;}
 uint64_t count=total(c);File file(out,resume);
 if(resume){require(file.size()>=s.offset,"Output is shorter than checkpoint");require(hash(file.tail(s.offset))==s.tailhash,"Output tail changed; resume refused");file.truncate(s.offset);file.flush();}
 else {file.flush();save(cp,s);}
 progress.total=count;progress.scanned=s.next;progress.written=s.lines;progress.bytes=s.offset;if(loaded)loaded(c);
 // Each worker holds at most about 1 MiB of generated text. Ordered batches keep output deterministic.
 size_t maxToken=1;for(auto& t:c.tokens)maxToken=std::max(maxToken,t.size());size_t maxLine=maxToken*c.max_length+c.prefix.size()+c.suffix.size()+1;
 uint64_t chunk=std::max<size_t>(1,std::min<size_t>(8192,(1024*1024)/maxLine));
 while(s.next<count&&!progress.stop){
  std::vector<std::future<std::pair<std::string,uint64_t>>> tasks;uint64_t end=s.next;
  for(unsigned w=0;w<c.threads&&end<count;++w){uint64_t begin=end;end+=std::min(chunk,count-end);uint64_t finish=end;
   tasks.push_back(std::async(std::launch::async,[&,begin,finish]{std::string text;text.reserve(std::min<uint64_t>(1024*1024,(finish-begin)*maxLine));uint64_t lines=0;for(uint64_t i=begin;i<finish;++i){auto s=candidate(c,i);if(accepts(c,s)){text+=s;text+='\n';++lines;}}return std::make_pair(std::move(text),lines);}));}
  State next=s;for(auto& task:tasks){auto part=task.get();require(next.offset<=uint64_t(INT64_MAX)-part.first.size(),"Output exceeds supported file size");file.write(part.first);next.offset+=part.first.size();next.lines+=part.second;}
  file.flush();next.next=end;next.tailhash=hash(file.tail(next.offset));save(cp,next);s=std::move(next);
  progress.scanned=s.next;progress.written=s.lines;progress.bytes=s.offset;
 }
}
}
