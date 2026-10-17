#include "engine.hpp"
#include "unique.hpp"
#include "checkpoint_retry.hpp"
#include "cpu_budget.hpp"
#include "gpu.hpp"
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <future>
#include <condition_variable>
#include <deque>
#include <mutex>
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
static void mergeTokens(Config& c,const std::vector<std::string>& selected){
 std::unordered_set<std::string> seen(c.tokens.begin(),c.tokens.end());
 for(const auto& token:selected)if(seen.insert(token).second)c.tokens.push_back(token);
}
void add_emojis(Config& c,const std::string& text){
 // Whitespace separates complete entries; never split a ZWJ/flag/modifier sequence.
 auto selected=tokenize(text,true);
 for(const auto& token:selected){characters(token);require(token.find('\0')==std::string::npos,"Emoji entries cannot contain NUL");}
 mergeTokens(c,selected);
}
void add_persian(Config& c){mergeTokens(c,tokenize(u8"ابپتثجچحخدذرزژسشصضطظعغفقکگلمنوهیآ",false));}
void add_shift_letters(Config& c){
 // Windows Persian (00000429), Unicode CLDR release-32 Windows keyboard table.
 // Only Persian letter keys are mapped; preserve all other scalar values.
 static const std::pair<std::string,std::string> mapping[]={{u8"ض",u8"ً"},{u8"ص",u8"ٌ"},{u8"ث",u8"ٍ"},{u8"ق",u8"ريال"},{u8"ف",u8"،"},{u8"غ",u8"؛"},{u8"ع",u8","},{u8"ه",u8"]"},{u8"خ",u8"["},{u8"ح",u8"\\"},{u8"ج",u8"}"},{u8"چ",u8"{"},{u8"ش",u8"َ"},{u8"س",u8"ُ"},{u8"ی",u8"ِ"},{u8"ب",u8"ّ"},{u8"ل",u8"ۀ"},{u8"ا",u8"آ"},{u8"ت",u8"ـ"},{u8"ن",u8"«"},{u8"م",u8"»"},{u8"ک",u8":"},{u8"گ",u8"\""},{u8"پ",u8"|"},{u8"ظ",u8"ة"},{u8"ط",u8"ي"},{u8"ز",u8"ژ"},{u8"ر",u8"ؤ"},{u8"ذ",u8"إ"},{u8"د",u8"أ"},{u8"ئ",u8"ء"},{u8"و",u8"<"}};
 auto original=c.tokens;std::vector<std::string> selected;
 for(const auto& token:original){
  std::string shifted;bool changed=false;
  for(size_t i=0;i<token.size();){size_t width=scalarWidth(token,i);auto scalar=token.substr(i,width);i+=width;bool found=false;
   for(const auto& entry:mapping)if(entry.first==scalar){shifted+=entry.second;changed=true;found=true;break;}
   if(!found)shifted+=scalar;
  }
  if(changed)selected.push_back(std::move(shifted));
 }
 mergeTokens(c,selected);
}
Index total(const Config& c){
 require(unsigned(c.compute_mode)<=unsigned(ComputeMode::GpuPreferred),"Invalid compute mode");
 require(c.resource_percent<=100,"Resource percentage must be 0..100");
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
 if(!(c.lower||c.upper||c.digit||c.symbol))return true;
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
static std::string serialize(const State& s){std::ostringstream o;o<<"PASSGEN_5\n"<<s.next<<' '<<s.lines<<' '<<s.offset<<' '<<s.tailhash<<'\n';auto& c=s.c;o<<c.min_length<<' '<<c.max_length<<' '<<c.threads<<' '<<c.lower<<' '<<c.upper<<' '<<c.digit<<' '<<c.symbol<<' '<<c.no_repeat<<' '<<c.character_lengths<<' '<<c.resource_percent<<' '<<unsigned(c.compute_mode)<<'\n'<<encode(c.prefix)<<' '<<encode(c.suffix)<<'\n'<<c.tokens.size()<<'\n';for(auto& t:c.tokens)o<<encode(t)<<'\n';return o.str();}
static void save(const fs::path& p,const State& s){auto tmp=p;tmp+=".tmp";std::error_code ec;fs::remove(tmp,ec);{File f(tmp,false);auto body=serialize(s);f.write(body+"CHECK "+std::to_string(hash(body))+"\n");f.flush();}replace(tmp,p);}
static State load(const fs::path& p){
 require(fs::file_size(p)<=16*1024*1024,"Checkpoint too large");std::ifstream f(p,std::ios::binary);std::string all((std::istreambuf_iterator<char>(f)),{});
 auto at=all.rfind("CHECK ");require(at!=std::string::npos,"Invalid checkpoint");auto body=all.substr(0,at);std::istringstream check(all.substr(at+6));uint64_t checksum=0;
 require(bool(check>>checksum)&&checksum==hash(body),"Checkpoint checksum mismatch");
 std::istringstream in(body);std::string magic,a,b;State s;size_t n=0;auto& c=s.c;
 require(bool(in>>magic>>s.next>>s.lines>>s.offset>>s.tailhash>>c.min_length>>c.max_length>>c.threads>>c.lower>>c.upper>>c.digit>>c.symbol>>c.no_repeat)&&(magic=="PASSGEN_1"||magic=="PASSGEN_2"||magic=="PASSGEN_3"||magic=="PASSGEN_4"||magic=="PASSGEN_5"),"Invalid checkpoint");
 if(magic=="PASSGEN_3"||magic=="PASSGEN_4"||magic=="PASSGEN_5")require(bool(in>>c.character_lengths),"Invalid checkpoint");
 if(magic=="PASSGEN_4"||magic=="PASSGEN_5")require(bool(in>>c.resource_percent),"Invalid resource policy");
 if(magic=="PASSGEN_5"){unsigned mode=0;require(bool(in>>mode)&&mode<=2,"Invalid compute mode");c.compute_mode=static_cast<ComputeMode>(mode);}
 require(bool(in>>a>>b>>n)&&n<=100000,"Invalid checkpoint");c.prefix=decode(a);c.suffix=decode(b);
 for(size_t i=0;i<n;++i){require(bool(in>>a),"Invalid checkpoint");c.tokens.push_back(decode(a));}
 auto count=total(c);require(s.next<=count&&s.lines<=s.next&&s.offset<=INT64_MAX,"Invalid checkpoint position");
 auto range=bounds(c);UniqueOutputs unique(c,range.minimum,range.maximum);
 require(magic=="PASSGEN_3"||magic=="PASSGEN_4"||magic=="PASSGEN_5"||!unique.needsCanonicalization(),"This older word checkpoint may contain duplicates. Start a new output with PassGen 0.1.2");
 return s;
}
// A run owns its workers; no thread creation per output block.
class Workers {
 std::mutex mutex;std::condition_variable ready;bool quitting=false;
 std::deque<std::function<void()>> jobs;std::vector<std::thread> threads;
 void close(){ {std::lock_guard<std::mutex> lock(mutex);quitting=true;}ready.notify_all();for(auto& thread:threads)if(thread.joinable())thread.join(); }
public:
 explicit Workers(unsigned count){try{for(unsigned i=0;i<count;++i)threads.emplace_back([this]{for(;;){std::function<void()> job;{std::unique_lock<std::mutex> lock(mutex);ready.wait(lock,[&]{return quitting||!jobs.empty();});if(jobs.empty())return;job=std::move(jobs.front());jobs.pop_front();}job();}});}catch(...){close();throw;}}
 ~Workers(){close();}
 template<class F>auto submit(F task){using T=decltype(task());auto work=std::make_shared<std::packaged_task<T()>>(std::move(task));auto result=work->get_future();{std::lock_guard<std::mutex> lock(mutex);jobs.emplace_back([work]{(*work)();});}ready.notify_one();return result;}
};
static unsigned classes(const std::string& text){
 unsigned mask=0;for(unsigned char ch:text){if(ch>='a'&&ch<='z')mask|=1;else if(ch>='A'&&ch<='Z')mask|=2;else if(ch>='0'&&ch<='9')mask|=4;else if(ch>=33&&ch<=126)mask|=8;}return mask;
}
// Metadata shared by workers; the odometer updates only the changed suffix.
struct Alphabet {
 std::vector<unsigned> masks;std::vector<size_t> chars;
 unsigned required,affixMask;size_t affixChars,width;bool fixed,uniformChars;
 explicit Alphabet(const Config& c):required(unsigned(c.lower)|(unsigned(c.upper)<<1)|(unsigned(c.digit)<<2)|(unsigned(c.symbol)<<3)),affixMask(classes(c.prefix)|classes(c.suffix)),affixChars(c.character_lengths?characters(c.prefix)+characters(c.suffix):0),width(c.tokens.front().size()),fixed(true),uniformChars(true){
  for(const auto& token:c.tokens){masks.push_back(classes(token));chars.push_back(c.character_lengths?characters(token):0);fixed&=token.size()==width;uniformChars&=chars.back()==chars.front();}
 }
};
struct RangeResult {std::string text;uint64_t lines=0,scanned=0;bool quota_hit=false,from_gpu=false;};
static RangeResult generateRange(const Config& c,Index begin,uint64_t span,const UniqueOutputs& unique,const Plan& plan,const Alphabet& alphabet,size_t maxLine,const std::atomic<bool>& stop,double cpu_fraction,uint64_t byte_limit=UINT64_MAX){
 RangeResult result;auto& text=result.text;text.reserve(size_t(std::min<uint64_t>(byte_limit,span*maxLine)));const size_t radix=c.tokens.size();
 CpuBudget cpu(cpu_fraction,stop);
 while(span){
  Index rank=begin;const Segment* segment=nullptr;
  for(const auto& part:plan.segments){if(rank<part.count){segment=&part;break;}rank-=part.count;}
  require(segment!=nullptr,"Candidate index out of range");
  Index power=1;unsigned length=0;
  for(unsigned n=1;n<=segment->tokens.maximum;++n){power*=uint32_t(radix);if(n<segment->tokens.minimum)continue;if(rank<power){length=n;break;}rank-=power;}
  require(length!=0,"Candidate index out of range");
  Index available=power-rank;uint64_t amount=available<Index(span)?available.u64():span;
  std::array<size_t,128> digits{};
  if(rank.fits64()){uint64_t value=rank.u64();for(unsigned i=length;i>0;--i){digits[i-1]=value%radix;value/=radix;}}
  else for(unsigned i=length;i>0;--i)digits[i-1]=rank.divide(uint32_t(radix));
  unsigned counts[4]={};size_t charCount=alphabet.affixChars;unsigned adjacent=0;
  if(c.character_lengths&&alphabet.uniformChars)charCount+=length*alphabet.chars.front();
  auto account=[&](size_t token,int delta){if(c.character_lengths&&!alphabet.uniformChars)charCount+=delta*alphabet.chars[token];if(alphabet.required)for(unsigned bit=0;bit<4;++bit)if(alphabet.masks[token]&(1u<<bit))counts[bit]+=delta;};
  std::string line=c.prefix;for(unsigned i=0;i<length;++i){account(digits[i],1);if(c.no_repeat&&i&&digits[i]==digits[i-1])++adjacent;if(alphabet.fixed)line+=c.tokens[digits[i]];}
  if(alphabet.fixed){line+=c.suffix;line+='\n';}
  // A uniform, prefix-free alphabet with no active filters has no per-result
  // validation work. Emit bounded groups while retaining the same odometer,
  // checkpoint rank order, stop interval, and complete-line quota boundary.
  const bool unrestricted=alphabet.fixed&&!unique.needsCanonicalization()&&!c.no_repeat
    &&(alphabet.required&~alphabet.affixMask)==0
    &&(!c.character_lengths||(alphabet.uniformChars&&charCount==segment->final_chars));
  if(unrestricted){
   uint64_t emitted=0;
   while(emitted<amount){
    cpu.pace();if(stop.load())return result;
    uint64_t requested=std::min<uint64_t>(2048,amount-emitted);
    uint64_t batch=std::min<uint64_t>(requested,(byte_limit-text.size())/line.size());
    for(uint64_t n=0;n<batch;++n){
     text+=line;
     if(emitted+n+1==amount)break;
     for(unsigned i=length;i>0;){--i;size_t next=digits[i]+1==radix?0:digits[i]+1;digits[i]=next;
      size_t offset=c.prefix.size()+i*alphabet.width;
      if(alphabet.width==1)line[offset]=c.tokens[next][0];
      else std::memcpy(&line[offset],c.tokens[next].data(),alphabet.width);
      if(next)break;
     }
    }
    emitted+=batch;result.lines+=batch;result.scanned+=batch;
    if(batch<requested){cpu.pace();result.quota_hit=true;return result;}
   }
   begin+=amount;span-=amount;continue;
  }
  for(uint64_t n=0;n<amount;++n){
   if((n&2047)==0){cpu.pace();if(stop.load())return result;}
   bool valid=(!c.no_repeat||adjacent==0)&&(!c.character_lengths||charCount==segment->final_chars);
   if(valid&&alphabet.required)for(unsigned bit=0;bit<4;++bit)if((alphabet.required&(1u<<bit))&&!(alphabet.affixMask&(1u<<bit))&&!counts[bit]){valid=false;break;}
   if(valid){
    if(alphabet.fixed&&!unique.needsCanonicalization()){
     if(line.size()>byte_limit-text.size()){cpu.pace();result.quota_hit=true;return result;}text+=line;
    }
    else {std::string body;for(unsigned i=0;i<length;++i)body+=c.tokens[digits[i]];if(unique.canonical(body,digits.data(),length)){
     size_t bytes=c.prefix.size()+body.size()+c.suffix.size()+1;
     if(bytes>byte_limit-text.size()){cpu.pace();result.quota_hit=true;return result;}
     text+=c.prefix;text+=body;text+=c.suffix;text+='\n';}else valid=false;}
    if(valid)++result.lines;
   }
   ++result.scanned;
   if(n+1==amount)break;
   for(unsigned i=length;i>0;){--i;size_t old=digits[i],next=old+1==radix?0:old+1;
    if(c.no_repeat){if(i&&digits[i-1]==old)--adjacent;if(i+1<length&&digits[i+1]==old)--adjacent;}
    account(old,-1);digits[i]=next;account(next,1);
    if(c.no_repeat){if(i&&digits[i-1]==next)++adjacent;if(i+1<length&&digits[i+1]==next)++adjacent;}
    if(alphabet.fixed)std::memcpy(&line[c.prefix.size()+i*alphabet.width],c.tokens[next].data(),alphabet.width);
    if(next)break;
   }
  }
  begin+=amount;span-=amount;
 }
 cpu.pace();return result;
}
struct GpuSlice {
 unsigned length=0;size_t span=0;std::vector<uint32_t> digits;
};
static GpuSlice gpuSlice(const Config& c,Index rank,uint64_t requested,const Plan& plan){
 const Segment* segment=nullptr;
 for(const auto& part:plan.segments){if(rank<part.count){segment=&part;break;}rank-=part.count;}
 require(segment!=nullptr,"GPU candidate index out of range");Index power=1;GpuSlice slice;
 for(unsigned n=1;n<=segment->tokens.maximum;++n){power*=uint32_t(c.tokens.size());if(n<segment->tokens.minimum)continue;if(rank<power){slice.length=n;break;}rank-=power;}
 require(slice.length!=0,"GPU candidate index out of range");auto remaining=power-rank;
 slice.span=size_t(remaining<Index(requested)?remaining.u64():requested);slice.digits.resize(slice.length);
 for(unsigned n=0;n<slice.length;++n)slice.digits[n]=rank.divide(uint32_t(c.tokens.size()));
 return slice;
}
static RangeResult generateGpuRange(GpuEngine& gpu,const Config& c,Index begin,uint64_t span,const Plan& plan,const Alphabet& alphabet,size_t maxLine,const std::atomic<bool>& stop,size_t testCap=0){
 RangeResult result;result.text.reserve(size_t(span)*maxLine);
 while(span&&!stop.load()){
  auto slice=gpuSlice(c,begin,span,plan);
  size_t row=c.prefix.size()+slice.length*alphabet.width+c.suffix.size()+1;
  size_t capacity=gpu.max_span(row);if(testCap)capacity=std::min(capacity,testCap);
  require(capacity>0,"GPU buffer cannot fit this output length");slice.span=std::min(slice.span,capacity);
  auto part=gpu.generate(slice.digits,slice.length,slice.span,alphabet.required,c.no_repeat,c.prefix,c.suffix,&stop,c.resource_percent?c.resource_percent:100);
  if(!part.span)break;
  require(part.span==slice.span,"Incomplete GPU batch");
  result.text+=part.text;result.lines+=part.lines;result.scanned+=part.span;result.from_gpu=true;
  begin+=part.span;span-=part.span;
 }
 return result;
}
// Choose a representative large-length range, not only the tiny length-one prefix.
static Index gpuProbeStart(const Config& c,const Plan& plan,const Index& resumed){
 Index start=0,chosen=0;
 for(const auto& segment:plan.segments){Index tokenStart=start,power=1;
  for(unsigned n=1;n<=segment.tokens.maximum;++n){power*=uint32_t(c.tokens.size());if(n<segment.tokens.minimum)continue;chosen=tokenStart;tokenStart+=power;}
  start+=segment.count;
 }
 return chosen<resumed?resumed:chosen;
}
void run(Config c,const std::string& output,bool resume,Progress& progress,const std::function<void(const Config&)>& loaded,const ResourceSnapshot* resource_snapshot,const GpuTestOptions* gpu_test){
 auto out=fs::absolute(path(output));auto cp=out;cp+=".pgstate";State s;
 if(resume){s=load(cp);c=s.c;}else {std::unordered_set<std::string> seen;c.tokens.erase(std::remove_if(c.tokens.begin(),c.tokens.end(),[&](const auto& t){return !seen.insert(t).second;}),c.tokens.end());total(c);require(!fs::exists(cp),"Checkpoint already exists; select another output or resume");s.c=c;}
 ResourcePlan limits;ResourceSnapshot snapshot;
 if(c.resource_percent||c.compute_mode!=ComputeMode::Cpu)snapshot=resource_snapshot?*resource_snapshot:detect_resources(output);
 if(c.resource_percent){
  require(snapshot.cpu_known&&snapshot.memory_known&&snapshot.disk_known,"Cannot identify CPU, RAM or free output space; choose manual mode or an accessible output folder");
  limits=plan_resources(snapshot,c.resource_percent);
  require(limits.memory_sufficient,"Not enough available RAM within the resource budget");
  c.threads=limits.workers;s.c=c;
 }
 Index count=total(c);Plan plan(c);UniqueOutputs unique(c,plan.tokens.minimum,plan.tokens.maximum);
 size_t maxToken=1;for(auto& t:c.tokens)maxToken=std::max(maxToken,t.size());
 size_t maxLine=maxToken*c.max_length+c.prefix.size()+c.suffix.size()+1;
 uint64_t buffer=c.resource_percent?limits.buffer_bytes_per_worker:4*1024*1024;
 require(!c.resource_percent||buffer>=maxLine,"A result line exceeds the available RAM buffer budget");
 progress.gpu_available=false;progress.gpu_used=false;progress.gpu_scanned=0;
 progress.compute_status.set("CPU selected");
 Alphabet alphabet(c);std::unique_ptr<GpuEngine> gpu;uint64_t gpuResultLimit=0;
 const size_t testCap=gpu_test?gpu_test->max_batch_candidates:0;
 if(c.compute_mode!=ComputeMode::Cpu&&s.next<count&&!progress.stop){
  progress.compute_status.set("Inspecting OpenCL GPU and driver...");
  if(!alphabet.fixed||(!alphabet.uniformChars&&c.character_lengths)||unique.needsCanonicalization())
   progress.compute_status.set("CPU: variable-width or ambiguous input");
  else if(c.compute_mode==ComputeMode::Automatic&&count-s.next<Index(8192)&&!(gpu_test&&gpu_test->skip_speed_selection))
   progress.compute_status.set("CPU: small job; GPU startup would add overhead");
  else {
   uint64_t reserve=(2+2*uint64_t(c.threads))*1024*1024;
   uint64_t ramBudget=c.resource_percent?limits.memory_budget_bytes:resource_detail::percentage(snapshot.memory_available_bytes,70);
   uint64_t cpuBuffers=buffer*c.threads;
   uint64_t hostBudget=ramBudget>reserve+cpuBuffers?std::min<uint64_t>(8*1024*1024,ramBudget-reserve-cpuBuffers):0;
   if(!snapshot.memory_known||hostBudget<256*1024)progress.compute_status.set("CPU: insufficient spare RAM for GPU transfer buffers");
   else try {
    std::vector<GpuToken> tokens;for(size_t n=0;n<c.tokens.size();++n)tokens.push_back({c.tokens[n],alphabet.masks[n]});
    gpuResultLimit=hostBudget/2;
    gpu.reset(new GpuEngine(std::move(tokens),size_t(hostBudget/2),gpu_test&&gpu_test->allow_cpu_opencl));
    if(!gpu->available()){progress.compute_status.set("CPU: "+gpu->status());gpu.reset();}
    else {
     progress.gpu_available=true;
     bool use=true;
     if(c.compute_mode==ComputeMode::Automatic&&!(gpu_test&&gpu_test->skip_speed_selection)){
      progress.compute_status.set("Calibrating CPU and OpenCL GPU...");
      Index probe=gpuProbeStart(c,plan,s.next);auto slice=gpuSlice(c,probe,65536,plan);
      size_t row=c.prefix.size()+slice.length*alphabet.width+c.suffix.size()+1;
      slice.span=std::min({slice.span,gpu->max_span(row),size_t(buffer/maxLine)});if(testCap)slice.span=std::min(slice.span,testCap);
      if(slice.span<256)use=false;
      else {
       auto warm=gpu->generate(slice.digits,slice.length,slice.span,alphabet.required,c.no_repeat,c.prefix,c.suffix,&progress.stop,100);
       auto started=std::chrono::steady_clock::now();
       auto reference=generateRange(c,probe,slice.span,unique,plan,alphabet,maxLine,progress.stop,1);
       auto cpuTime=std::chrono::steady_clock::now()-started;
       if(!progress.stop)require(warm.span==reference.scanned&&warm.text==reference.text,"GPU calibration output mismatch");
       std::string().swap(warm.text);
       started=std::chrono::steady_clock::now();
       auto measured=gpu->generate(slice.digits,slice.length,slice.span,alphabet.required,c.no_repeat,c.prefix,c.suffix,&progress.stop,100);
       auto gpuTime=std::chrono::steady_clock::now()-started;
       if(progress.stop)use=false;
       else {require(reference.scanned==measured.span&&reference.text==measured.text,"GPU calibration output mismatch");use=gpuTime*1.05<cpuTime;}
      }
     }
     if(use)progress.compute_status.set("CPU + "+gpu->status());
     else {progress.compute_status.set(progress.stop?"Stopped during compute selection":"CPU: GPU calibration did not improve transfer-inclusive speed");gpu.reset();}
    }
   }catch(const std::exception& error){progress.compute_status.set(std::string("CPU fallback: ")+error.what());gpu.reset();}
  }
 }
 File file(out,resume);
 if(resume){require(file.size()>=s.offset,"Output is shorter than checkpoint");require(hash(file.tail(s.offset))==s.tailhash,"Output tail changed; resume refused");file.truncate(s.offset);file.flush();}
 else {file.flush();save(cp,s);}
 progress.disk_limit=false;progress.active_workers=c.threads;progress.buffer_bytes=c.resource_percent?limits.buffer_bytes_per_worker:4*1024*1024;progress.disk_budget_bytes=c.resource_percent?limits.disk_budget_bytes:UINT64_MAX;
 progress.total=count;progress.scanned=s.next;progress.written=s.lines;progress.bytes=s.offset;if(loaded)loaded(c);
 // Bounded 4 MiB worker buffers, persistent workers, and identical rank order.
 uint64_t chunk=std::max<size_t>(1,std::min<size_t>(unique.needsCanonicalization()?8192:262144,size_t(buffer/maxLine)));
 const uint64_t initialOffset=s.offset;
 const double cpuFraction=c.resource_percent?limits.cpu_fraction:1;

 if(s.next==count||progress.stop)return;
 size_t gpuBatches=0;
 Workers workers(c.threads);std::unique_ptr<Workers> gpuWorker;if(gpu)gpuWorker.reset(new Workers(1));
 while(s.next<count&&!progress.stop){
  struct Task {Index begin;uint64_t span;std::future<RangeResult> result;bool gpu=false;};
  std::vector<Task> tasks;Index end=s.next;
  if(gpu){
   auto slice=gpuSlice(c,end,uint64_t(chunk),plan);
   size_t row=c.prefix.size()+slice.length*alphabet.width+c.suffix.size()+1;
   size_t maximum=gpu->max_span(row);if(testCap)maximum=std::min(maximum,testCap);
   if(maximum){uint64_t span=std::min<uint64_t>(slice.span,gpuResultLimit/maxLine);if(testCap)span=std::min<uint64_t>(span,testCap);if(!span)span=std::min<size_t>(slice.span,maximum);Index begin=end;end+=span;
    const bool injectedFailure=gpu_test&&gpuBatches++>=gpu_test->fail_after_batches;
    tasks.push_back({begin,span,gpuWorker->submit([&,begin,span,injectedFailure]{if(injectedFailure)throw std::runtime_error("Injected OpenCL recovery test failure");return generateGpuRange(*gpu,c,begin,span,plan,alphabet,maxLine,progress.stop,testCap);}),true});
   }else {progress.compute_status.set("CPU: GPU buffer does not fit the next output length");gpu.reset();}
  }
  for(unsigned w=0;w<c.threads&&end<count;++w){Index begin=end;auto remaining=count-end;uint64_t span=remaining<Index(chunk)?remaining.u64():chunk;end+=span;
   tasks.push_back({begin,span,workers.submit([&,begin,span]{return generateRange(c,begin,span,unique,plan,alphabet,maxLine,progress.stop,cpuFraction);})});}
  State next=s;uint64_t gpuCommitted=0;
  for(auto& task:tasks){
   RangeResult part;
   try {part=task.result.get();}
   catch(const std::exception& error){
    if(!task.gpu)throw;
    progress.compute_status.set(std::string("CPU fallback after GPU error: ")+error.what());gpu.reset();
    for(auto& pending:tasks)if(pending.result.valid())pending.result.wait();
    part=generateRange(c,task.begin,task.span,unique,plan,alphabet,maxLine,progress.stop,cpuFraction);
   }
   if(c.resource_percent){
    uint64_t used=next.offset-initialOffset;
    uint64_t available=limits.disk_budget_bytes>used?limits.disk_budget_bytes-used:0;
    if(part.text.size()>available){
     // Finish other workers before quota replay, so replay cannot add a fourth
     // CPU consumer to the configured three-worker generation budget.
     for(auto& later:tasks)if(later.result.valid())later.result.wait();
     std::string().swap(part.text);
     part=generateRange(c,task.begin,task.span,unique,plan,alphabet,maxLine,progress.stop,cpuFraction,available);
    }
   }
   require(next.offset<=uint64_t(INT64_MAX)-part.text.size(),"Output exceeds supported file size");
   if(part.from_gpu)gpuCommitted+=part.scanned;
   file.write(part.text);next.offset+=part.text.size();next.lines+=part.lines;next.next=task.begin+part.scanned;
   if(part.quota_hit){progress.disk_limit=true;progress.stop=true;}
   if(part.scanned<task.span)break;
  }
  // Drain queued work before the next batch or final return. Only the contiguous
  // committed rank prefix is kept; later partial worker results are discarded.
  for(auto& task:tasks)if(task.result.valid())task.result.get();
  file.flush();next.tailhash=hash(file.tail(next.offset));save(cp,next);s=std::move(next);
  if(gpuCommitted){progress.gpu_used=true;progress.gpu_scanned.fetch_add(gpuCommitted);}
  progress.scanned=s.next;progress.written=s.lines;progress.bytes=s.offset;

 }
}
}
