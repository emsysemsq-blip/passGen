#include "engine.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <thread>
#include <stdexcept>
#include <chrono>
#include <unordered_set>
namespace fs=std::filesystem;
static int checks=0;
static void check(bool v,const char* msg){++checks;if(!v)throw std::runtime_error(msg);}
template<class F>static void rejects(F f,const char* msg){bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}check(rejected,msg);}
static std::string read(const fs::path& p){std::ifstream f(p,std::ios::binary);return std::string((std::istreambuf_iterator<char>(f)),{});}
static void append(const fs::path& p,const std::string& text){std::ofstream f(p,std::ios::app|std::ios::binary);f<<text;}
int main(){fs::path dir=fs::temp_directory_path()/("passgen-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));fs::create_directories(dir);try{
 pg::Config extras;extras.tokens={"a","A"};pg::add_shift_letters(extras);check(extras.tokens.size()==2,"shift alphabet stays unique");
 extras.tokens={"ab",u8"بa"};pg::add_shift_letters(extras);check(extras.tokens==std::vector<std::string>({"ab",u8"بa","AB",u8"بA"}),"shift adds uppercase words without changing Persian");
 extras.tokens={u8"ی"};pg::add_persian(extras);check(extras.tokens.size()==33,"Persian preset has 32 letters plus alef madda without duplicates");
 extras={};pg::add_emojis(extras,u8"👩‍💻 👍🏽 🇮🇷 👩‍💻");check(extras.tokens==std::vector<std::string>({u8"👩‍💻",u8"👍🏽",u8"🇮🇷"}),"manual emoji preserves sequences and deduplicates entries");
 extras.max_length=2;extras.threads=2;pg::Progress ep;pg::run(extras,(dir/"emoji.txt").u8string(),false,ep);check(ep.written==12&&read(dir/"emoji.txt").find(u8"👩‍💻👍🏽\n")!=std::string::npos,"complete emoji combinations stay intact");
 extras={};pg::add_emojis(extras,u8"😀 😀😀");extras.max_length=2;pg::Progress ea;pg::run(extras,(dir/"emoji-alias.txt").u8string(),false,ea);check(ea.written==4,"ambiguous emoji aliases never produce duplicate lines");
 extras={};pg::add_emojis(extras,u8"👩‍💻");extras.character_lengths=true;extras.min_length=extras.max_length=3;pg::Progress ec;ec.stop=true;pg::run(extras,(dir/"emoji-resume.txt").u8string(),false,ec);pg::Progress er;pg::run({},(dir/"emoji-resume.txt").u8string(),true,er);check(read(dir/"emoji-resume.txt")==u8"👩‍💻\n","emoji sequence and scalar length preserved on resume");
 pg::Config c;c.tokens=pg::tokenize("aab",false);c.max_length=2;
 check(c.tokens.size()==2,"deduplicate characters");check(pg::total(c)==6,"complete count");check(pg::candidate(c,0)=="a"&&pg::candidate(c,5)=="bb","index boundaries");
 pg::Progress p;pg::run(c,(dir/"one.txt").u8string(),false,p);check(read(dir/"one.txt")=="a\nb\naa\nab\nba\nbb\n","exact order, no conditions");check(p.scanned.load()==6&&p.written==6&&p.bytes==16,"progress counters");
 c.threads=4;pg::Progress parallel;pg::run(c,(dir/"parallel.txt").u8string(),false,parallel);check(read(dir/"parallel.txt")==read(dir/"one.txt"),"parallel determinism");
 c.no_repeat=true;pg::Progress nr;pg::run(c,(dir/"nr.txt").u8string(),false,nr);check(read(dir/"nr.txt")=="a\nb\nab\nba\n","adjacent duplicate filter");
 auto tokens=pg::tokenize(u8"آبآ",false);check(tokens.size()==2&&tokens[0]==u8"آ","Unicode codepoints");c={};c.tokens=tokens;c.max_length=2;pg::Progress unicode;pg::run(c,(dir/fs::u8path(u8"فارسی.txt")).u8string(),false,unicode);check(unicode.written==6,"Unicode output path and count");check(read(dir/fs::u8path(u8"فارسی.txt")).find(u8"آب\n")!=std::string::npos,"Unicode byte preservation");
 c={};c.tokens=pg::tokenize("red blue red",true);c.max_length=2;check(pg::total(c)==6&&pg::candidate(c,3)=="redblue","word concatenation");
 c={};c.tokens={"a","A","1","!"};c.min_length=4;c.max_length=4;c.lower=c.upper=c.digit=c.symbol=true;pg::Progress filtered;pg::run(c,(dir/"filtered.txt").u8string(),false,filtered);check(filtered.written==24,"all class requirements");
 c={};c.tokens={"b"};c.lower=c.upper=c.digit=c.symbol=true;c.prefix="A1!";check(pg::accepts(c,pg::candidate(c,0)),"prefix participates in rules");
 c={};c.tokens={"a","b"};c.max_length=2;pg::Progress stopped;pg::run(c,(dir/"resume.txt").u8string(),false,stopped,[&](const pg::Config&){stopped.stop=true;});check(stopped.scanned.load()==0&&fs::exists(dir/"resume.txt.pgstate"),"stop creates durable initial checkpoint");
 append(dir/"resume.txt","uncommitted partial data");pg::Progress resumed;pg::run({},(dir/"resume.txt").u8string(),true,resumed);check(read(dir/"resume.txt")==read(dir/"one.txt"),"resume removes uncommitted tail and restores config");
 pg::Progress completed;pg::run({},(dir/"resume.txt").u8string(),true,completed);check(read(dir/"resume.txt")==read(dir/"one.txt")&&completed.written==6,"completed resume is idempotent");
 rejects([&]{pg::Progress q;pg::run(c,(dir/"one.txt").u8string(),false,q);},"never overwrite existing output");
 append(dir/"resume.txt.pgstate","CHECK 1\n");rejects([&]{pg::Progress q;pg::run({},(dir/"resume.txt").u8string(),true,q);},"corrupt checkpoint rejected");
 {std::fstream f(dir/"one.txt",std::ios::in|std::ios::out|std::ios::binary);f.put('x');}rejects([&]{pg::Progress q;pg::run({},(dir/"one.txt").u8string(),true,q);},"changed tail rejected");
 fs::resize_file(dir/"parallel.txt",0);rejects([&]{pg::Progress q;pg::run({},(dir/"parallel.txt").u8string(),true,q);},"truncated output rejected");
 c.max_length=64;check(pg::total(c).str()=="36893488147419103230","count beyond uint64 supported");c={};c.tokens={"a","b"};c.min_length=c.max_length=65;
 check(pg::total(c).str()=="36893488147419103232","exact 2^65 count");
 check(pg::candidate(c,pg::total(c)-1)==std::string(65,'b'),"candidate rank beyond uint64");
 check(pg::candidate(c,pg::Index(UINT64_MAX)+1)=="b"+std::string(64,'a'),"candidate at exact 2^64 boundary");
 c.min_length=c.max_length=128;check(pg::total(c).str()=="340282366920938463463374607431768211456","exact 2^128 count");
 check(pg::candidate(c,pg::total(c)-1)==std::string(128,'b'),"candidate beyond 128 bits");
 pg::Progress huge;huge.stop=true;pg::run(c,(dir/"huge.txt").u8string(),false,huge);check(huge.total.load()==pg::total(c)&&huge.scanned.load()==0,"large new job starts and stops without output expansion");
 check((pg::total(c)-1).fractionOf(pg::total(c))>0.99,"large progress fraction finite");
 c.tokens.clear();rejects([&]{pg::total(c);},"empty input rejected");
 rejects([]{pg::tokenize(std::string("\xc0\x80",2),false);},"invalid UTF8 rejected");rejects([]{pg::tokenize(std::string("\xed\xa0\x80",3),false);},"surrogate rejected");rejects([]{pg::tokenize("a\nb",false);},"embedded newlines rejected");
 c={};c.tokens={"a","b"};c.max_length=19;c.threads=2;pg::Progress middle;std::atomic<bool> quit{false};std::thread stopper([&]{while(!quit&&!middle.scanned.load())std::this_thread::yield();if(!quit)middle.stop=true;});try{pg::run(c,(dir/"middle.txt").u8string(),false,middle);}catch(...){quit=true;stopper.join();throw;}quit=true;stopper.join();check(middle.scanned.load()>0&&middle.scanned.load()<pg::total(c),"stop after committed nonempty batch");auto partial=read(dir/"middle.txt");append(dir/"middle.txt","simulated crash tail\n");pg::Progress recovery;pg::run({},(dir/"middle.txt").u8string(),true,recovery);check(recovery.scanned.load()==pg::total(c)&&recovery.written.load()==pg::total(c),"nonempty checkpoint resumed completely");auto actual=read(dir/"middle.txt");check(actual.compare(0,partial.size(),partial)==0,"committed prefix retained");std::string expected;for(uint64_t i=0;i<pg::total(c);++i){expected+=pg::candidate(c,i);expected+='\n';}check(actual==expected,"resumed output exactly matches full indexed sequence");
 c={};c.tokens={"a","aa","b"};c.max_length=10;c.threads=2;
 pg::Progress wordStop;std::atomic<bool> wordDone{false};std::thread wordWatcher([&]{while(!wordDone&&!wordStop.scanned.load())std::this_thread::yield();if(!wordDone)wordStop.stop=true;});
 try{pg::run(c,(dir/"words-resume.txt").u8string(),false,wordStop);}catch(...){wordDone=true;wordWatcher.join();throw;}wordDone=true;wordWatcher.join();
 check(wordStop.scanned.load()>0&&wordStop.scanned.load()<pg::total(c),"ambiguous word job stops at nonempty checkpoint");
 append(dir/"words-resume.txt","uncommitted word tail");pg::Progress wordResume;pg::run({},(dir/"words-resume.txt").u8string(),true,wordResume);
 pg::Progress wordFull;pg::run(c,(dir/"words-full.txt").u8string(),false,wordFull);
 auto wordOutput=read(dir/"words-resume.txt");check(wordOutput==read(dir/"words-full.txt"),"ambiguous word resume matches complete run exactly");
 std::istringstream wordLines(wordOutput);std::string wordLine;std::unordered_set<std::string> seenWords;bool uniqueWords=true;while(std::getline(wordLines,wordLine))uniqueWords&=seenWords.insert(wordLine).second;
 check(uniqueWords&&seenWords.size()==wordResume.written.load(),"no duplicates across ambiguous word batches or resume");
 c={};c.tokens={"a","a","b"};c.min_length=2;c.max_length=3;pg::Progress duplicateInput;pg::run(c,(dir/"duplicate-input.txt").u8string(),false,duplicateInput);
 check(duplicateInput.written.load()==12&&duplicateInput.total.load()==12,"engine API deduplicates input before assigning ranks");
 c={};c.tokens={"e","l","I",std::string(20,'x')};c.character_lengths=true;c.min_length=14;c.max_length=30;
 bool oldPreviewEmpty=true;for(unsigned i=0;i<1000;++i)oldPreviewEmpty&=pg::candidate(c,i).empty();
 check(oldPreviewEmpty,"mixed-word reproduction has no matches in its first 1000 ranks");
 auto samples=pg::preview(c);check(!samples.samples.empty()&&samples.inspected<=4096,"representative preview finds later valid ranks within budget");
 bool validSamples=true;std::unordered_set<std::string> sampleSet;
 for(const auto& text:samples.samples)validSamples&=pg::characters(text)>=14&&pg::characters(text)<=30&&pg::accepts(c,text)&&sampleSet.insert(text).second;
 check(validSamples,"representative samples satisfy final lengths, rules and uniqueness");
 c.tokens={"a","b"};c.upper=true;auto noSamples=pg::preview(c);check(noSamples.samples.empty()&&noSamples.inspected<=4096,"impossible ASCII filter has a bounded empty preview");
 c={};c.tokens={"a","!"};pg::add_symbols(c,"!!@@!");
 check(c.tokens==std::vector<std::string>({"a","!","@"}),"custom symbol alphabet merges and deduplicates exact input");
 check(!c.symbol&&pg::accepts(c,"a"),"custom symbols do not require a symbol in every password");
 auto beforeEmpty=c.tokens;pg::add_symbols(c,"");check(c.tokens==beforeEmpty,"empty symbol input leaves the alphabet unchanged");
 c.character_lengths=true;c.max_length=2;pg::Progress symbolOutput;pg::run(c,(dir/"manual-symbols.txt").u8string(),false,symbolOutput);
 check(symbolOutput.written.load()==12&&read(dir/"manual-symbols.txt").find("aa\n")!=std::string::npos,"optional symbols preserve plain passwords and full combinations");
 auto beforeInvalid=c.tokens;rejects([&]{pg::add_symbols(c,"! A");},"symbol field rejects letters and whitespace");check(c.tokens==beforeInvalid,"invalid symbols leave the input alphabet untouched");
 rejects([&]{pg::add_symbols(c,"1");},"digits belong in main input instead of manual symbols");
 c={};c.tokens={"a"};c.character_lengths=true;pg::add_symbols(c,u8"€؟€");pg::Progress unicodeSymbols;pg::run(c,(dir/"unicode-symbols.txt").u8string(),false,unicodeSymbols);
 check(read(dir/"unicode-symbols.txt")==u8"a\n€\n؟\n","manual Unicode symbols are exact characters without a mandatory-symbol filter");
 c={};c.tokens={"a"};pg::add_symbols(c,"!@");c.max_length=3;pg::Progress saveSymbols;saveSymbols.stop=true;pg::run(c,(dir/"symbols-resume.txt").u8string(),false,saveSymbols);
 bool restoredAlphabet=false;pg::Progress resumeSymbols;pg::run({},(dir/"symbols-resume.txt").u8string(),true,resumeSymbols,[&](const pg::Config& saved){restoredAlphabet=saved.tokens==std::vector<std::string>({"a","!","@"})&&!saved.symbol;});
 check(restoredAlphabet&&resumeSymbols.written.load()==39,"checkpoint preserves optional manual-symbol alphabet");
 c={};c.tokens={"a","!"};c.max_length=2;c.symbol=true;pg::Progress oldSymbol;oldSymbol.stop=true;pg::run(c,(dir/"legacy-symbol.txt").u8string(),false,oldSymbol);
 pg::Progress oldResume;pg::run({},(dir/"legacy-symbol.txt").u8string(),true,oldResume);
 check(read(dir/"legacy-symbol.txt")=="!\na!\n!a\n!!\n","legacy checkpoints retain their original required-ASCII-symbol rule");
 fs::remove_all(dir);std::cout<<"PASS: "<<checks<<" checks\n";return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<" checks: "<<e.what()<<" | artifacts "<<dir<<"\n";return 1;}}
