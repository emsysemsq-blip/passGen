#ifndef UNICODE
#define UNICODE
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <chrono>
#include <thread>
#include "ui_ids.hpp"
using namespace pg::ui;
static std::wstring get(HWND h){int n=(int)SendMessageW(h,WM_GETTEXTLENGTH,0,0);std::wstring s(n+1,0);SendMessageW(h,WM_GETTEXT,n+1,(LPARAM)s.data());s.resize(n);return s;}
static void require(bool v,const char* message){if(!v)throw std::runtime_error(message);}
int main(int argc,char**){PROCESS_INFORMATION process{};HWND h=nullptr;std::filesystem::path out,large,wordOutput,symbolOutput,wordSymbolOutput,extraOutput;try{STARTUPINFOW si{};si.cb=sizeof(si);wchar_t command[]=L"PassGen.exe";require(CreateProcessW(nullptr,command,nullptr,nullptr,FALSE,0,nullptr,nullptr,&si,&process),"Launch GUI");WaitForInputIdle(process.hProcess,10000);for(int i=0;i<200&&!h;++i){h=FindWindowW(L"PassGenWindow",nullptr);std::this_thread::sleep_for(std::chrono::milliseconds(25));}require(h,"Find GUI");
 auto field=[&](int id,const std::wstring& s){require(SetWindowTextW(GetDlgItem(h,id),s.c_str()),"Set GUI field");};
 auto click=[&](int id){SendMessageW(GetDlgItem(h,id),BM_CLICK,0,0);};
 auto wait=[&](const wchar_t* token){for(int i=0;i<2400;++i){if(get(GetDlgItem(h,Status)).find(token)!=std::wstring::npos)return;std::this_thread::sleep_for(std::chrono::milliseconds(25));}throw std::runtime_error("GUI operation timed out");};
 wchar_t temp[MAX_PATH];GetTempPathW(MAX_PATH,temp);out=std::filesystem::path(temp)/(L"passgen-gui-"+std::to_wstring(GetTickCount64())+L".txt");
 field(Input,L"\x0622\x0628");field(Output,out.wstring());click(Preview);require(get(GetDlgItem(h,Sample)).find(L"Candidates: 6")!=std::wstring::npos,"Preview default count");click(Start);wait(L"Complete.");std::ifstream f(out,std::ios::binary);std::string actual((std::istreambuf_iterator<char>(f)),{});f.close();require(actual==u8"آ\nب\nآآ\nآب\nبآ\nبب\n","GUI Unicode output exact");click(Resume);wait(L"Complete.");
 field(Input,L"ab");field(Max,L"20");large=out;large+=L".large.txt";field(Output,large.wstring());click(Start);click(Stop);wait(L"Stopped safely.");click(Resume);wait(L"Complete.");std::ifstream big(large,std::ios::binary);std::string line,last;uint64_t count=0;while(std::getline(big,line)){last=line;++count;}big.close();require(count==2097150&&last==std::string(20,'b'),"GUI stop/resume exact count and final candidate");
 SendMessageW(GetDlgItem(h,Words),BM_SETCHECK,BST_CHECKED,0);field(Input,L"a aa b");field(Min,L"3");field(Max,L"4");field(Prefix,L"A");field(Suffix,L"1");wordOutput=out;wordOutput+=L".words.txt";field(Output,wordOutput.wstring());click(Start);wait(L"Complete.");
 std::ifstream wordsFile(wordOutput,std::ios::binary);std::string wordData((std::istreambuf_iterator<char>(wordsFile)),{});wordsFile.close();require(wordData=="Aa1\nAb1\nAaa1\nAab1\nAba1\nAbb1\n","GUI final-character lengths and unique word aliases");
 SendMessageW(GetDlgItem(h,Words),BM_SETCHECK,BST_UNCHECKED,0);field(Min,L"");field(Prefix,L"");field(Suffix,L"");
 field(Input,L"ab");field(Symbol,L"!@!!");field(Min,L"1");field(Max,L"2");symbolOutput=out;symbolOutput+=L".symbols.txt";field(Output,symbolOutput.wstring());click(Preview);
 require(get(GetDlgItem(h,Sample)).find(L"Candidates: 20")!=std::wstring::npos,"manual symbol edit extends alphabet without duplicate tokens");
 click(Start);wait(L"Complete.");std::ifstream symbolFile(symbolOutput,std::ios::binary);std::string symbolData((std::istreambuf_iterator<char>(symbolFile)),{});symbolFile.close();
 require(symbolData=="a\nb\n!\n@\naa\nab\na!\na@\nba\nbb\nb!\nb@\n!a\n!b\n!!\n!@\n@a\n@b\n@!\n@@\n","GUI optional manual symbols generate exact chosen alphabet including plain passwords");
 field(Symbol,L"$");click(Resume);wait(L"Complete.");std::ifstream resumedSymbolFile(symbolOutput,std::ios::binary);std::string resumedSymbolData((std::istreambuf_iterator<char>(resumedSymbolFile)),{});resumedSymbolFile.close();
 require(resumedSymbolData==symbolData,"Resume restores original symbols instead of replacing them with edited UI input");
 SendMessageW(GetDlgItem(h,Words),BM_SETCHECK,BST_CHECKED,0);field(Input,L"a aa");field(Symbol,L"!");wordSymbolOutput=out;wordSymbolOutput+=L".word-symbols.txt";field(Output,wordSymbolOutput.wstring());click(Start);wait(L"Complete.");
 std::ifstream mixedFile(wordSymbolOutput,std::ios::binary);std::string mixedData((std::istreambuf_iterator<char>(mixedFile)),{});mixedFile.close();
 require(mixedData=="a\n!\naa\na!\n!a\n!!\n","manual symbols combine with word tokens while preserving plain words and unique output");
 SendMessageW(GetDlgItem(h,Words),BM_SETCHECK,BST_UNCHECKED,0);
 field(Symbol,L"");field(Min,L"");
 field(Input,L"a");field(Emoji,L"😀 👩‍💻 😀");field(Min,L"1");field(Max,L"1");SendMessageW(GetDlgItem(h,TokenLengths),BM_SETCHECK,BST_CHECKED,0);SendMessageW(GetDlgItem(h,Persian),BM_SETCHECK,BST_CHECKED,0);SendMessageW(GetDlgItem(h,ShiftLetters),BM_SETCHECK,BST_CHECKED,0);
 extraOutput=out;extraOutput+=L".extras.txt";field(Output,extraOutput.wstring());click(Preview);require(get(GetDlgItem(h,Sample)).find(L"Candidates: 65")!=std::wstring::npos,"GUI merges emoji Persian and shift pools uniquely");click(Start);wait(L"Complete.");
 std::ifstream extraFile(extraOutput,std::ios::binary);std::string extraData((std::istreambuf_iterator<char>(extraFile)),{});extraFile.close();require(extraData.find(u8"👩‍💻\n")!=std::string::npos&&extraData.find(u8"ی\n")!=std::string::npos&&extraData.find(u8"َ\n")!=std::string::npos,"GUI extra alphabet output preserves complete emoji Persian Shift");
 field(Emoji,L"");SendMessageW(GetDlgItem(h,TokenLengths),BM_SETCHECK,BST_UNCHECKED,0);SendMessageW(GetDlgItem(h,Persian),BM_SETCHECK,BST_UNCHECKED,0);SendMessageW(GetDlgItem(h,ShiftLetters),BM_SETCHECK,BST_UNCHECKED,0);field(Min,L"");
 field(Input,L"abcdefghijklmnopqrstuvwxyz");field(Max,L"");click(Preview);require(get(GetDlgItem(h,Sample)).find(L"Candidates: 6402364363415443603228541259936211926")!=std::wstring::npos,"Default alphabet count beyond 64 bits");
 SendMessageW(GetDlgItem(h,Words),BM_SETCHECK,BST_CHECKED,0);field(Input,L"e l I xxxxxxxxxxxxxxxxxxxx");field(Min,L"14");field(Max,L"30");click(Preview);
 auto representative=get(GetDlgItem(h,Sample));require(representative.find(L"Representative samples")!=std::wstring::npos&&representative.find(std::wstring(14,L'e'))!=std::wstring::npos,"GUI representative preview finds matches beyond the old first-1000 window");
 SendMessageW(GetDlgItem(h,Words),BM_SETCHECK,BST_UNCHECKED,0);field(Min,L"");
 if(argc>1){field(Input,L"ab12");field(Max,L"4");click(Preview);std::cout<<"SCREENSHOT_READY\n"<<std::flush;std::this_thread::sleep_for(std::chrono::seconds(15));}
 PostMessageW(h,WM_CLOSE,0,0);require(WaitForSingleObject(process.hProcess,10000)==WAIT_OBJECT_0,"GUI closes");CloseHandle(process.hThread);CloseHandle(process.hProcess);for(auto& p:{out,large,wordOutput,symbolOutput,wordSymbolOutput,extraOutput}){std::filesystem::remove(p);std::filesystem::remove(p.wstring()+L".pgstate");}std::cout<<"PASS: GUI optional manual symbols and persisted alphabet, later-rank preview, unique words and final lengths, large-space and default-alphabet preview, Unicode generation, stop, resume, exact count, clean close\n";return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<"\n";if(process.hProcess)TerminateProcess(process.hProcess,1);return 1;}}
