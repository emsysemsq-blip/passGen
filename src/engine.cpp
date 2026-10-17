#include "engine.hpp"
#include "unique.hpp"
#include "checkpoint_retry.hpp"
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
static size_t scalarWidth(const std::string& text,size_t i){
  unsigned char c=text[i];size_t n=c<0x80?1:(c>=0xC2&&c<=0xDF?2:(c>=0xE0&&c<=0xEF?3:(c>=0xF0&&c<=0xF4?4:0)));
  require(n && i+n<=text.size(),"Invalid UTF-8 input");
  for(size_t j=1;j<n;++j)require((static_cast<unsigned char>(text[i+j])&0xC0)==0x80,"Invalid UTF-8 input");
  if(n==3)require(!((c==0xE0&&(unsigned char)text[i+1]<0xA0)||(c==0xED&&(unsigned char)text[i+1]>=0xA0)),"Invalid Unicode scalar");
  if(n==4)require(!((c==0xF0&&(unsigned char)text[i+1]<0x90)||(c==0xF4&&(unsigned char)text[i+1]>=0x90)),"Invalid Unicode scalar");
 return n;
}
size_t characters(const std::string& text){size_t count=0;for(size_t i=0;i<text.size();++count)i+=scalarWidth(text,i);return count;}
unsigned default_max_length(const Config& c){
 if(!c.character_lengths)return unsigned(c.tokens.size());
 size_t longest=1;for(const auto& token:c.tokens)longest=std::max(longest,characters(token));
 size_t defaultLength=longest*c.tokens.size()+characters(c.prefix)+characters(c.suffix);
 return unsigned(std::max<size_t>(1,std::min<size_t>(128,defaultLength)));
}
struct Bounds {unsigned minimum=1,maximum=0;};
static Bounds bounds(const Config& c,unsigned finalLength=0){
 if(!c.character_lengths)return {c.min_length,c.max_length};
 size_t affix=characters(c.prefix)+characters(c.suffix);
 unsigned minLength=finalLength?finalLength:c.min_length,maxLength=finalLength?finalLength:c.max_length;
 if(affix>=maxLength)return {};
 size_t smallest=SIZE_MAX,largest=0;
 for(const auto& token:c.tokens){size_t n=characters(token);smallest=std::min(smallest,n);largest=std::max(largest,n);}
 size_t minBody=minLength>affix?minLength-affix:1;
 size_t maxBody=maxLength-affix;
 return {unsigned(std::max<size_t>(1,(minBody+largest-1)/largest)),unsigned(maxBody/smallest)};
}
struct Segment {unsigned final_chars;Bounds tokens;Index count;};
struct Plan {
 Bounds tokens;
 std::vector<Segment> segments;
 Index count;
 explicit Plan(const Config& c):tokens(bounds(c)){
  std::vector<Index> powers(tokens.maximum+1,1);
  for(unsigned n=1;n<=tokens.maximum;++n){powers[n]=powers[n-1];powers[n]*=uint32_t(c.tokens.size());}
  auto add=[&](unsigned chars,Bounds r){Index subtotal;for(unsigned n=r.minimum;n<=r.maximum;++n)subtotal+=powers[n];if(subtotal){segments.push_back({chars,r,subtotal});count+=subtotal;}};
  if(c.character_lengths)for(unsigned length=c.min_length;length<=c.max_length;++length)add(length,bounds(c,length));
  else add(0,tokens);
 }
};
std::vector<std::string> tokenize(const std::string& text,bool words){
 std::vector<std::string> out; std::unordered_set<std::string> seen;
 auto add=[&](std::string s){if(!s.empty() && seen.insert(s).second)out.push_back(std::move(s));};
 if(words){std::istringstream in(text);std::string s;while(in>>s)add(s);}
 else for(size_t i=0;i<text.size();){
  size_t n=scalarWidth(text,i);unsigned char c=text[i];
  require(c!='\n'&&c!='\r'&&c!=0,"Input cannot contain newline or NUL");add(text.substr(i,n));i+=n;
 }
 return out;
}
static std::vector<std::string> parseSymbols(const std::string& text){
 auto symbols=tokenize(text,false);
 for(const auto& token:symbols){
  uint32_t cp=(unsigned char)token[0];
  if(token.size()>1){cp&=token.size()==2?0x1f:token.size()==3?0x0f:0x07;for(size_t i=1;i<token.size();++i)cp=(cp<<6)|((unsigned char)token[i]&0x3f);}
  bool asciiLetterOrDigit=(cp>='a'&&cp<='z')||(cp>='A'&&cp<='Z')||(cp>='0'&&cp<='9');
  bool whitespace=cp<=32||(cp>=0x7f&&cp<=0xa0)||cp==0x1680||(cp>=0x2000&&cp<=0x200a)||cp==0x2028||cp==0x2029||cp==0x202f||cp==0x205f||cp==0x3000;
  require(!asciiLetterOrDigit&&!whitespace,"Enter literal symbols together, e.g. !@#; no ASCII letters, digits, spaces or control characters");
 }
 return symbols;
}
void add_symbols(Config& c,const std::string& text){
 auto selected=parseSymbols(text);if(selected.empty())return;std::unordered_set<std::string> existing(c.tokens.begin(),c.tokens.end());
 for(const auto& token:selected){if(existing.insert(token).second)c.tokens.push_back(token);}
}
Index total(const Config& c){
 require(c.tokens.size()<=100000,"Too many input tokens (maximum 100000)");
 require(!c.tokens.empty(),"Enter at least one character or word");
 require(c.min_length>=1 && c.max_length>=c.min_length && c.max_length<=128,"Length must be 1..128 and minimum <= maximum");
 require(c.threads>=1 && c.threads<=32,"Worker count must be 1..32");
 size_t longest=0;for(auto& s:c.tokens){require(!s.empty()&&s.find_first_of("\r\n") == std::string::npos && s.find('\0')==std::string::npos,"Invalid token");longest=std::max(longest,s.size());}
 require(longest<=4096 && c.prefix.size()<=4096 && c.suffix.size()<=4096,"Token, prefix or suffix too long");
 require(c.prefix.find_first_of("\r\n")==std::string::npos && c.suffix.find_first_of("\r\n")==std::string::npos,"Prefix/suffix cannot contain newlines");
 return Plan(c).count;
}
static std::string generate(const Config& c,Index index,const UniqueOutputs& unique,const Plan& plan){
 const Segment* segment=nullptr;
 for(const auto& part:plan.segments){if(index<part.count){segment=&part;break;}index-=part.count;}
 require(segment!=nullptr,"Candidate index out of range");
 Index power=1;unsigned len=0;for(unsigned l=1;l<=segment->tokens.maximum;++l){power*=c.tokens.size();if(l>=segment->tokens.minimum){if(index<power){len=l;break;}index-=power;}}
 require(len!=0,"Candidate index out of range");
 std::array<size_t,128> digits{};
 if(index.fits64()){uint64_t small=index.u64();for(unsigned i=len;i>0;--i){digits[i-1]=small%c.tokens.size();small/=c.tokens.size();}}
 else for(unsigned i=len;i>0;--i)digits[i-1]=index.divide(uint32_t(c.tokens.size()));
 if(c.no_repeat)for(unsigned i=1;i<len;++i)if(digits[i]==digits[i-1])return {};
 std::string body;size_t outputLength=unique.affixCharacters();
 for(unsigned i=0;i<len;++i){body+=c.tokens[digits[i]];outputLength+=unique.tokenCharacters(digits[i]);}
 if(c.character_lengths&&outputLength!=segment->final_chars)return {};
 if(!unique.canonical(body,digits.data(),len))return {};
 return c.prefix+body+c.suffix;
}
std::string candidate(const Config& c,Index index){Plan plan(c);UniqueOutputs unique(c,plan.tokens.minimum,plan.tokens.maximum);return generate(c,std::move(index),unique,plan);}
Preview preview(const Config& c,size_t maximum){
 total(c);Plan plan(c);UniqueOutputs unique(c,plan.tokens.minimum,plan.tokens.maximum);
 Preview result;std::unordered_set<std::string> ranks,texts;
 constexpr size_t budget=4096;
 auto sample=[&](Index rank){
  if(result.samples.size()>=maximum||result.inspected>=budget||rank>=plan.count)return;
  if(!ranks.insert(rank.str()).second)return;
  ++result.inspected;auto text=generate(c,std::move(rank),unique,plan);
  if(accepts(c,text)&&texts.insert(text).second)result.samples.push_back(std::move(text));
 };
 // Visit token-length boundaries, including long strings of short tokens that
 // a sequential first-1000 scan misses in mixed-length word alphabets.
 Index segmentStart=0;
 for(const auto& segment:plan.segments){
  Index power=1,tokenStart=segmentStart,cycling=0;
  for(unsigned n=1;n<=segment.tokens.maximum;++n){
   power*=uint32_t(c.tokens.size());cycling*=uint32_t(c.tokens.size());cycling+=(n-1)%c.tokens.size();
   if(n<segment.tokens.minimum)continue;
   sample(tokenStart);sample(tokenStart+power-1);sample(tokenStart+cycling);
   tokenStart+=power;
   if(result.samples.size()>=maximum||result.inspected>=budget)break;
  }
  segmentStart+=segment.count;
  if(result.samples.size()>=maximum||result.inspected>=budget)break;
 }
 for(uint64_t n=0;n<1000&&Index(n)<plan.count&&result.samples.size()<maximum&&result.inspected<budget;++n)sample(n);
 return result;
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
#ifdef _WIN32
static std::runtime_error windowsError(const char* action,DWORD code){
 wchar_t* buffer=nullptr;
 DWORD size=FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,nullptr,code,0,(LPWSTR)&buffer,0,nullptr);
 std::string detail;
 if(size){int bytes=WideCharToMultiByte(CP_UTF8,0,buffer,(int)size,nullptr,0,nullptr,nullptr);detail.resize(bytes);WideCharToMultiByte(CP_UTF8,0,buffer,(int)size,detail.data(),bytes,nullptr,nullptr);LocalFree(buffer);}
 while(!detail.empty()&&(detail.back()=='\r'||detail.back()=='\n'||detail.back()==' '))detail.pop_back();
 return std::runtime_error(std::string(action)+" (Windows error "+std::to_string(code)+(detail.empty()?"":": "+detail)+"). Keep the output and any .pgstate/.pgstate.tmp files together for recovery; check file locks, folder permissions and free disk space before Resume");
}
#endif
static void replace(const fs::path& a,const fs::path& b){
#ifdef _WIN32
 DWORD error=retryCheckpointReplace([&]()->uint32_t{
  if(MoveFileExW(a.c_str(),b.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))return ERROR_SUCCESS;
  return GetLastError();
 },[](unsigned milliseconds){Sleep(milliseconds);});
 if(error)throw windowsError("Checkpoint replace failed",error);
#else
 fs::rename(a,b);auto parent=b.parent_path();int d=open(parent.empty()?".":parent.c_str(),O_RDONLY|O_DIRECTORY);require(d>=0,"Checkpoint directory open failed");int result=fsync(d);close(d);require(result==0,"Checkpoint directory flush failed");
#endif
}
static std::string encode(const std::string& s){static const char h[]="0123456789abcdef";std::string out;for(unsigned char b:s){out+=h[b>>4];out+=h[b&15];}return out.empty()?"-":out;}
static std::string decode(const std::string& s){if(s=="-")return {};require(s.size()%2==0,"Invalid checkpoint");std::string out;auto val=[](char c){if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;throw std::runtime_error("Invalid checkpoint");};for(size_t i=0;i<s.size();i+=2)out+=char(val(s[i])*16+val(s[i+1]));return out;}
struct State{Config c;Index next=0;uint64_t lines=0,offset=0,tailhash=hash("");};
static std::string serialize(const State& s){std::ostringstream o;o<<"PASSGEN_3\n"<<s.next<<' '<<s.lines<<' '<<s.offset<<' '<<s.tailhash<<'\n';auto& c=s.c;o<<c.min_length<<' '<<c.max_length<<' '<<c.threads<<' '<<c.lower<<' '<<c.upper<<' '<<c.digit<<' '<<c.symbol<<' '<<c.no_repeat<<' '<<c.character_lengths<<'\n'<<encode(c.prefix)<<' '<<encode(c.suffix)<<'\n'<<c.tokens.size()<<'\n';for(auto& t:c.tokens)o<<encode(t)<<'\n';return o.str();}
static void save(const fs::path& p,const State& s){auto tmp=p;tmp+=".tmp";std::error_code ec;fs::remove(tmp,ec);{File f(tmp,false);auto body=serialize(s);f.write(body+"CHECK "+std::to_string(hash(body))+"\n");f.flush();}replace(tmp,p);}
static State load(const fs::path& p){
 require(fs::file_size(p)<=16*1024*1024,"Checkpoint too large");std::ifstream f(p,std::ios::binary);std::string all((std::istreambuf_iterator<char>(f)),{});
 auto at=all.rfind("CHECK ");require(at!=std::string::npos,"Invalid checkpoint");auto body=all.substr(0,at);std::istringstream check(all.substr(at+6));uint64_t checksum=0;
 require(bool(check>>checksum)&&checksum==hash(body),"Checkpoint checksum mismatch");
 std::istringstream in(body);std::string magic,a,b;State s;size_t n=0;auto& c=s.c;
 require(bool(in>>magic>>s.next>>s.lines>>s.offset>>s.tailhash>>c.min_length>>c.max_length>>c.threads>>c.lower>>c.upper>>c.digit>>c.symbol>>c.no_repeat)&&(magic=="PASSGEN_1"||magic=="PASSGEN_2"||magic=="PASSGEN_3"),"Invalid checkpoint");
 if(magic=="PASSGEN_3")require(bool(in>>c.character_lengths),"Invalid checkpoint");
 require(bool(in>>a>>b>>n)&&n<=100000,"Invalid checkpoint");c.prefix=decode(a);c.suffix=decode(b);
 for(size_t i=0;i<n;++i){require(bool(in>>a),"Invalid checkpoint");c.tokens.push_back(decode(a));}
 auto count=total(c);require(s.next<=count&&s.lines<=s.next&&s.offset<=INT64_MAX,"Invalid checkpoint position");
 auto range=bounds(c);UniqueOutputs unique(c,range.minimum,range.maximum);
 require(magic=="PASSGEN_3"||!unique.needsCanonicalization(),"This older word checkpoint may contain duplicates. Start a new output with PassGen 0.1.2");
 return s;
}
void run(Config c,const std::string& output,bool resume,Progress& progress,const std::function<void(const Config&)>& loaded){
 auto out=fs::absolute(path(output));auto cp=out;cp+=".pgstate";State s;
 if(resume){s=load(cp);c=s.c;}else {std::unordered_set<std::string> seen;c.tokens.erase(std::remove_if(c.tokens.begin(),c.tokens.end(),[&](const auto& t){return !seen.insert(t).second;}),c.tokens.end());total(c);require(!fs::exists(cp),"Checkpoint already exists; select another output or resume");s.c=c;}
 Index count=total(c);Plan plan(c);UniqueOutputs unique(c,plan.tokens.minimum,plan.tokens.maximum);File file(out,resume);
 if(resume){require(file.size()>=s.offset,"Output is shorter than checkpoint");require(hash(file.tail(s.offset))==s.tailhash,"Output tail changed; resume refused");file.truncate(s.offset);file.flush();}
 else {file.flush();save(cp,s);}
 progress.total=count;progress.scanned=s.next;progress.written=s.lines;progress.bytes=s.offset;if(loaded)loaded(c);
 // Each worker holds at most about 1 MiB of generated text. Ordered batches keep output deterministic.
 size_t maxToken=1;for(auto& t:c.tokens)maxToken=std::max(maxToken,t.size());size_t maxLine=maxToken*c.max_length+c.prefix.size()+c.suffix.size()+1;
 uint64_t chunk=std::max<size_t>(1,std::min<size_t>(8192,(1024*1024)/maxLine));
 while(s.next<count&&!progress.stop){
  std::vector<std::future<std::pair<std::string,uint64_t>>> tasks;Index end=s.next;
  for(unsigned w=0;w<c.threads&&end<count;++w){Index begin=end;auto remaining=count-end;uint64_t span=remaining<Index(chunk)?remaining.u64():chunk;end+=span;Index finish=end;
   tasks.push_back(std::async(std::launch::async,[&,begin,finish,span]{std::string text;text.reserve(std::min<uint64_t>(1024*1024,span*maxLine));uint64_t lines=0;for(Index i=begin;i<finish;++i){auto s=generate(c,i,unique,plan);if(accepts(c,s)){text+=s;text+='\n';++lines;}}return std::make_pair(std::move(text),lines);}));}
  State next=s;for(auto& task:tasks){auto part=task.get();require(next.offset<=uint64_t(INT64_MAX)-part.first.size(),"Output exceeds supported file size");file.write(part.first);next.offset+=part.first.size();next.lines+=part.second;}
  file.flush();next.next=end;next.tailhash=hash(file.tail(next.offset));save(cp,next);s=std::move(next);
  progress.scanned=s.next;progress.written=s.lines;progress.bytes=s.offset;
 }
}
}
