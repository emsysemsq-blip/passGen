#include "gpu.hpp"
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

static unsigned checks=0;
static void check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
template<class F> static void rejects(F operation,const char* message){bool threw=false;try{operation();}catch(const std::exception&){threw=true;}check(threw,message);}
static unsigned mask(const std::string& text){
 unsigned value=0;
 for(unsigned char c:text){if(c>='a'&&c<='z')value|=1;if(c>='A'&&c<='Z')value|=2;if(c>='0'&&c<='9')value|=4;if((c>=33&&c<=47)||(c>=58&&c<=64)||(c>=91&&c<=96)||(c>=123&&c<=126))value|=8;}
 return value;
}
static std::vector<pg::GpuToken> tokens(const std::vector<std::string>& texts){
 std::vector<pg::GpuToken> result;for(const auto& text:texts)result.push_back({text,mask(text)});return result;
}
struct Expected{std::string text;uint64_t lines=0;};
// Independent host oracle: a right-to-left odometer, rather than kernel rank arithmetic.
static Expected oracle(const std::vector<std::string>& alphabet,std::vector<uint32_t> little,size_t count,unsigned required,bool adjacent,const std::string& prefix,const std::string& suffix){
 std::vector<uint32_t> digits(little.rbegin(),little.rend());Expected expected;
 for(size_t row=0;row<count;++row){
  std::string text=prefix;bool accepted=true;
  for(size_t i=0;i<digits.size();++i){if(adjacent&&i&&digits[i]==digits[i-1])accepted=false;text+=alphabet.at(digits[i]);}
  text+=suffix;accepted=accepted&&((mask(text)&required)==required);
  if(accepted){expected.text+=text+'\n';++expected.lines;}
  if(row+1==count)break;
  bool carried=true;for(size_t i=digits.size();i--;){if(++digits[i]<alphabet.size()){carried=false;break;}digits[i]=0;}
  if(carried)throw std::runtime_error("Oracle range crossed a length boundary");
 }
 return expected;
}
static void compare(pg::GpuEngine& engine,const std::vector<std::string>& alphabet,const std::vector<uint32_t>& start,size_t count,unsigned required=0,bool adjacent=false,const std::string& prefix="",const std::string& suffix=""){
 const auto expected=oracle(alphabet,start,count,required,adjacent,prefix,suffix);
 const auto actual=engine.generate(start,unsigned(start.size()),count,required,adjacent,prefix,suffix,nullptr,100);
 check(actual.span==count,"A completed OpenCL launch accounts for every raw candidate");
 check(actual.text==expected.text,"Real OpenCL kernel bytes exactly match the independent host oracle");
 check(actual.lines==expected.lines,"Real OpenCL accepted-line count agrees with host oracle");
 check(actual.device_seconds>=0,"OpenCL device timing is nonnegative");
}
int main(int argc,char** argv){try{
 const bool required=argc>1&&std::string(argv[1])=="--require-opencl";
 const auto production=pg::query_gpu();
 for(const auto& device:production.devices)check(device.gpu,"Production discovery never promotes a CPU OpenCL device to a GPU");
 auto strict=pg::GpuEngine(tokens({"a","b"}),4*1024*1024);
 if(production.devices.empty())check(!strict.available(),"No GPU hardware means the production backend remains unavailable");
 const auto testing=pg::query_gpu(true);
 pg::GpuEngine ascii(tokens({"a","b","C","1","!"}),8*1024*1024,true);
 if(!ascii.available()){
  check(!testing.status.empty()||!ascii.status().empty(),"Absent OpenCL device reports a diagnostic");
  if(required)throw std::runtime_error("--require-opencl requested an actual OpenCL execution, but no test device is available: "+ascii.status());
  std::cout<<"SKIP actual OpenCL execution: "<<ascii.status()<<"; "<<checks<<" fallback/discovery checks passed\n";return 0;
 }
 std::cout<<"Actual OpenCL test device: "<<ascii.name()<<" (CPU OpenCL is explicitly allowed only in this test)\n";
 check(!testing.devices.empty(),"Available test backend appears in diagnostic inventory");
 const std::vector<std::string> alphabet={"a","b","C","1","!"};
 compare(ascii,alphabet,{0,0,0,0,0},3125);
 compare(ascii,alphabet,{4,3,2,1,0},997,15,true,"(",")");
 compare(ascii,alphabet,{0,0,0},125,15,false,"A9!","");
 compare(ascii,alphabet,{4,4,4,4,4},1,0,false,"pre",u8"پایان");
 compare(ascii,alphabet,{4,4,3,0},26,0,true);
 const std::vector<std::string> lower={"a","b"};pg::GpuEngine binary(tokens(lower),8*1024*1024,true);
 compare(binary,lower,std::vector<uint32_t>(14,0),16384);
 compare(binary,lower,std::vector<uint32_t>(10,0),1024,2); // Every row rejected.
 compare(binary,lower,std::vector<uint32_t>(10,0),1024,0,true); // Only alternating rows survive.
 auto boundary=std::vector<uint32_t>(65,0);boundary[64]=1;compare(binary,lower,boundary,17);
 auto veryLarge=std::vector<uint32_t>(128,0);veryLarge[100]=1;veryLarge[63]=1;veryLarge[0]=1;compare(binary,lower,veryLarge,10000,0,false,u8"آ",u8"😀");
 auto finalRanks=std::vector<uint32_t>(128,1);finalRanks[0]=0;finalRanks[1]=0;compare(binary,lower,finalRanks,4);
 const std::vector<std::string> persian={u8"آ",u8"ب",u8"ی"};pg::GpuEngine persianEngine(tokens(persian),4*1024*1024,true);check(persianEngine.available(),"Fixed-width Persian alphabet is supported");
 compare(persianEngine,persian,{0,0,0,0},81,0,true,"A1",u8"!");
 const std::vector<std::string> emoji={u8"😀",u8"🚀",u8"🧪"};pg::GpuEngine emojiEngine(tokens(emoji),4*1024*1024,true);check(emojiEngine.available(),"Four-byte emoji alphabet is supported");compare(emojiEngine,emoji,{2,2,0,0,0},201,0,false,u8"ی",u8"آ");
 const std::vector<std::string> composite={u8"👩‍💻",u8"👨‍💻"};pg::GpuEngine compositeEngine(tokens(composite),4*1024*1024,true);check(compositeEngine.available(),"Equal-byte-width composite emoji remain intact");compare(compositeEngine,composite,{0,0,0,0},16,0,true);
 // ASCII classes must be derived from bytes, not trusted from caller metadata.
 pg::GpuEngine wrongMasks({{"a",0},{"A",0},{"1",0},{"!",0}},1024*1024,true);check(wrongMasks.available(),"Untrusted ASCII mask metadata is recomputed");compare(wrongMasks,{"a","A","1","!"},{0,0,0,0},256,15);
 std::atomic<bool> stop{true};const auto cancelled=binary.generate({0,0},2,4,0,false,"","",&stop,70);check(cancelled.span==0&&cancelled.text.empty()&&cancelled.lines==0,"A pre-stopped launch performs no work");
 rejects([&]{binary.generate({2,0},2,1,0,false,"","");},"Out-of-alphabet seed digit is refused");
 rejects([&]{binary.generate({0},2,1,0,false,"","");},"Seed width mismatch is refused");
 rejects([&]{binary.generate({1,1},2,2,0,false,"","");},"Range crossing the final rank is refused");
 pg::GpuEngine mixed(tokens({"a",u8"ب"}),1024*1024,true);check(!mixed.available()&&!mixed.status().empty(),"Variable-byte-width alphabet requests the CPU path with a reason");
 pg::GpuEngine duplicates(tokens({"a","a"}),1024*1024,true);check(!duplicates.available(),"Duplicate GPU tokens are refused to preserve injectivity");
 pg::GpuEngine empty({},1024*1024,true);check(!empty.available(),"An empty GPU alphabet is refused");
 pg::GpuEngine oversized(tokens({std::string(129,'a')}),1024*1024,true);check(!oversized.available(),"Oversized GPU tokens use the CPU path");
 check(binary.max_span(1024*1024*16)==0,"Host output budget refuses an oversized row before allocating it");
 check(binary.max_span(16)>0&&binary.max_span(16)<=65536,"GPU batches have a finite bounded candidate span");
 std::cout<<"PASS: "<<checks<<" actual OpenCL and discovery checks\n";return 0;
 }catch(const std::exception& error){std::cerr<<"FAIL after "<<checks<<" checks: "<<error.what()<<"\n";return 1;}}
