#ifndef UNICODE
#define UNICODE
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commctrl.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <chrono>
#include <thread>
#include <vector>
#include "ui_ids.hpp"
using namespace pg::ui;
static std::wstring get(HWND h){int n=(int)SendMessageW(h,WM_GETTEXTLENGTH,0,0);std::wstring s(n+1,0);SendMessageW(h,WM_GETTEXT,n+1,(LPARAM)s.data());s.resize(n);return s;}
static void require(bool v,const char* message){if(!v)throw std::runtime_error(message);}
int main(int argc,char**){PROCESS_INFORMATION process{};HWND h=nullptr;std::filesystem::path out,large,wordOutput,symbolOutput,wordSymbolOutput,extraOutput,limitOutput,customOutput;try{STARTUPINFOW si{};si.cb=sizeof(si);wchar_t command[]=L"PassGen.exe";require(CreateProcessW(nullptr,command,nullptr,nullptr,FALSE,0,nullptr,nullptr,&si,&process),"Launch GUI");WaitForInputIdle(process.hProcess,10000);for(int i=0;i<200&&!h;++i){h=FindWindowW(L"PassGenWindow",nullptr);std::this_thread::sleep_for(std::chrono::milliseconds(25));}require(h,"Find GUI");
 auto field=[&](int id,const std::wstring& s){require(SetWindowTextW(GetDlgItem(h,id),s.c_str()),"Set GUI field");};
 auto click=[&](int id){SendMessageW(GetDlgItem(h,id),BM_CLICK,0,0);};
 auto wait=[&](const wchar_t* token){for(int i=0;i<2400;++i){if(get(GetDlgItem(h,Status)).find(token)!=std::wstring::npos)return;std::this_thread::sleep_for(std::chrono::milliseconds(25));}throw std::runtime_error("GUI operation timed out");};
 // Queue real key messages through the application's dialog message loop.
 field(Input,L"FirstWord");HWND input=GetDlgItem(h,Input);
 SendMessageW(input,EM_SETSEL,9,9);auto beforeEnter=get(GetDlgItem(h,Status));
 require(GetWindowLongPtrW(input,GWL_STYLE)&ES_WANTRETURN,"Input accepts plain Enter");
 PostMessageW(input,WM_KEYDOWN,VK_RETURN,1);PostMessageW(input,WM_KEYUP,VK_RETURN,0xc0000001);
 std::this_thread::sleep_for(std::chrono::milliseconds(200));
 require(get(input)==L"FirstWord\r\n","Plain Enter inserts a new line through IsDialogMessage");
 for(wchar_t c:std::wstring(L"SecondWord"))PostMessageW(input,WM_CHAR,c,1);
 std::this_thread::sleep_for(std::chrono::milliseconds(600));
 require(get(input)==L"FirstWord\r\nSecondWord","Typing continues on the next line");
 require(SendMessageW(GetDlgItem(h,TokenList),LVM_GETITEMCOUNT,0,0)==2,"New manually typed line updates token table");
 require(get(GetDlgItem(h,Status))==beforeEnter&&IsWindowEnabled(GetDlgItem(h,Start)),"Enter does not start generation");
 require(SendMessageW(GetDlgItem(h,Words),BM_GETCHECK,0,0)==BST_CHECKED,"Whole-word input defaults on");SendMessageW(GetDlgItem(h,Words),BM_SETCHECK,BST_UNCHECKED,0);
 require(SendMessageW(GetDlgItem(h,Compute),CB_GETCURSEL,0,0)==0,"Automatic CPU/GPU selection defaults on");
 require(SendMessageW(GetDlgItem(h,Resource70),BM_GETCHECK,0,0)==BST_CHECKED,"Auto 70 defaults on");require(!IsWindowEnabled(GetDlgItem(h,Threads)),"Auto mode controls worker count");
 auto resources=get(GetDlgItem(h,ResourceInfo));require(resources.find(L"CPU ")!=std::wstring::npos&&resources.find(L"RAM free")!=std::wstring::npos&&resources.find(L"Disk free")!=std::wstring::npos,"Resource discovery shown in GUI");
 SendMessageW(GetDlgItem(h,Words),BM_SETCHECK,BST_CHECKED,0);field(Input,L"100000001\r\n200000002\r\n300000003\r\nUserWord\r\nuserWord\r\nUSERWORD");std::this_thread::sleep_for(std::chrono::milliseconds(600));require(SendMessageW(GetDlgItem(h,TokenList),LVM_GETITEMCOUNT,0,0)==6,"Multiline pasted custom words replace all default table rows without edit notifications");
 SendMessageW(GetDlgItem(h,Words),BM_SETCHECK,BST_UNCHECKED,0);std::this_thread::sleep_for(std::chrono::milliseconds(600));require(SendMessageW(GetDlgItem(h,TokenList),LVM_GETITEMCOUNT,0,0)==0,"Invalid character-mode multiline input clears stale token rows");require(get(GetDlgItem(h,Sample)).find(L"Whole words")!=std::wstring::npos,"Invalid multiline input explains the correct mode");
 wchar_t temp[MAX_PATH];GetTempPathW(MAX_PATH,temp);out=std::filesystem::path(temp)/(L"passgen-gui-"+std::to_wstring(GetTickCount64())+L".txt");
 SendMessageW(GetDlgItem(h,Words),BM_SETCHECK,BST_CHECKED,0);SendMessageW(GetDlgItem(h,TokenLengths),BM_SETCHECK,BST_CHECKED,0);field(Min,L"1");field(Max,L"2");std::this_thread::sleep_for(std::chrono::milliseconds(600));
 HWND customList=GetDlgItem(h,TokenList),customCap=GetDlgItem(h,TokenCap);SendMessageW(customList,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(250,30));SendMessageW(customList,WM_LBUTTONUP,0,MAKELPARAM(250,30));SendMessageW(customList,WM_KEYDOWN,VK_HOME,0);for(int n=0;n<5;++n)SendMessageW(customList,WM_KEYDOWN,VK_DOWN,0);require(SendMessageW(customList,LVM_GETNEXTITEM,WPARAM(-1),LVNI_SELECTED)==5,"Last custom word row selected and scrolled into view");
 require(IsWindowVisible(customCap),"The last custom word has a visible editable dropdown after scrolling");
 SendMessageW(customCap,CB_SETCURSEL,1,0);SendMessageW(h,WM_COMMAND,MAKEWPARAM(TokenCap,CBN_SELCHANGE),(LPARAM)customCap);customOutput=out;customOutput+=L".custom.txt";field(Output,customOutput.wstring());click(Start);wait(L"Complete.");std::ifstream customFile(customOutput,std::ios::binary);std::string customData((std::istreambuf_iterator<char>(customFile)),{});customFile.close();require(std::count(customData.begin(),customData.end(),'\n')==41&&customData.find("USERWORDUSERWORD\n")==std::string::npos&&customData.find("100000001USERWORD\n")!=std::string::npos,"User-supplied words receive their own editable caps, not preset characters");
 SendMessageW(GetDlgItem(h,Words),BM_SETCHECK,BST_UNCHECKED,0);SendMessageW(GetDlgItem(h,TokenLengths),BM_SETCHECK,BST_UNCHECKED,0);field(Min,L"");field(Max,L"");
 field(Input,L"\x0622\x0628");field(Output,out.wstring());click(Preview);require(get(GetDlgItem(h,Sample)).find(L"Candidates: 6")!=std::wstring::npos,"Preview default count");click(Start);wait(L"Complete.");std::ifstream f(out,std::ios::binary);std::string actual((std::istreambuf_iterator<char>(f)),{});f.close();require(actual==u8"آ\nب\nآآ\nآب\nبآ\nبب\n","GUI Unicode output exact");click(Resume);wait(L"Complete.");
 click(Resource70);require(IsWindowEnabled(GetDlgItem(h,Threads)),"Manual mode enables workers");
 field(Input,L"ab");field(Max,L"20");large=out;large+=L".large.txt";field(Output,large.wstring());click(Start);click(Stop);wait(L"Stopped safely.");click(Resume);wait(L"Complete.");std::ifstream big(large,std::ios::binary);std::string line,last;uint64_t count=0;while(std::getline(big,line)){last=line;++count;}big.close();require(count==2097150&&last==std::string(20,'b'),"GUI stop/resume exact count and final candidate");
 click(KnownEnabled);SendMessageW(GetDlgItem(h,KnownPosition),CB_SETCURSEL,2,0);SendMessageW(h,WM_COMMAND,MAKEWPARAM(KnownPosition,CBN_SELCHANGE),0);
 SendMessageW(GetDlgItem(h,Words),BM_SETCHECK,BST_CHECKED,0);field(Input,L"a aa b");field(Min,L"3");field(Max,L"4");field(Prefix,L"A");field(Suffix,L"1");wordOutput=out;wordOutput+=L".words.txt";field(Output,wordOutput.wstring());click(Start);wait(L"Complete.");
 std::ifstream wordsFile(wordOutput,std::ios::binary);std::string wordData((std::istreambuf_iterator<char>(wordsFile)),{});wordsFile.close();require(wordData=="Aa1\nAb1\nAaa1\nAab1\nAba1\nAbb1\n","GUI final-character lengths and unique word aliases");
 click(KnownEnabled);SendMessageW(GetDlgItem(h,Words),BM_SETCHECK,BST_UNCHECKED,0);field(Min,L"");field(Prefix,L"");field(Suffix,L"");
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
 SendMessageW(GetDlgItem(h,Words),BM_SETCHECK,BST_CHECKED,0);SendMessageW(GetDlgItem(h,TokenLengths),BM_SETCHECK,BST_CHECKED,0);field(Input,L"red blue");field(Min,L"1");field(Max,L"3");std::this_thread::sleep_for(std::chrono::milliseconds(400));
 HWND tokenList=GetDlgItem(h,TokenList),capCombo=GetDlgItem(h,TokenCap);require(SendMessageW(tokenList,LVM_GETITEMCOUNT,0,0)==2,"Limit rows follow unique word inputs");require(SendMessageW(capCombo,CB_GETCOUNT,0,0)==11,"Always plus 1..10 choices");
 SendMessageW(tokenList,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(250,30));SendMessageW(tokenList,WM_LBUTTONUP,0,MAKELPARAM(250,30));SendMessageW(tokenList,WM_KEYDOWN,VK_HOME,0);SendMessageW(tokenList,WM_KEYUP,VK_HOME,0);require(SendMessageW(tokenList,LVM_GETNEXTITEM,WPARAM(-1),LVNI_SELECTED)==0,"First word selected for its cap");require(IsWindowVisible(capCombo),"Selecting a token displays its dropdown");
 SendMessageW(capCombo,CB_SETCURSEL,2,0);SendMessageW(h,WM_COMMAND,MAKEWPARAM(TokenCap,CBN_SELCHANGE),(LPARAM)capCombo);
 limitOutput=out;limitOutput+=L".limits.txt";field(Output,limitOutput.wstring());click(Start);wait(L"Complete.");
 auto readLimits=[&]{std::ifstream file(limitOutput,std::ios::binary);return std::string((std::istreambuf_iterator<char>(file)),{});};
 const std::string capped="red\nblue\nredred\nredblue\nbluered\nblueblue\nredredblue\nredbluered\nredblueblue\nblueredred\nblueredblue\nbluebluered\nblueblueblue\n";
 require(readLimits()==capped,"Per-word limit permits zero through two uses and keeps unlimited alternatives");
 SendMessageW(capCombo,CB_SETCURSEL,0,0);SendMessageW(h,WM_COMMAND,MAKEWPARAM(TokenCap,CBN_SELCHANGE),(LPARAM)capCombo);click(Resume);wait(L"Complete.");require(readLimits()==capped,"Resume preserves saved word caps despite form edits");
 SendMessageW(GetDlgItem(h,Words),BM_SETCHECK,BST_UNCHECKED,0);SendMessageW(GetDlgItem(h,TokenLengths),BM_SETCHECK,BST_UNCHECKED,0);
 // Each selected input source works independently; inactive fields are ignored.
 std::vector<std::filesystem::path> placementOutputs;
 auto generateCase=[&](const char* name,const std::string& expected){auto path=out;path+=std::string(".")+name+".txt";placementOutputs.push_back(path);field(Output,path.wstring());click(Start);wait(L"Complete.");std::ifstream file(path,std::ios::binary);std::string data((std::istreambuf_iterator<char>(file)),{});require(data==expected,name);};
 auto mode=[&](int value){SendMessageW(GetDlgItem(h,KnownPosition),CB_SETCURSEL,value,0);SendMessageW(h,WM_COMMAND,MAKEWPARAM(KnownPosition,CBN_SELCHANGE),0);};
 field(Input,L"ab");field(Min,L"4");field(Max,L"4");field(Prefix,L"Am1");field(Suffix,L"Am1");click(KnownEnabled);
 mode(0);require(!IsWindowEnabled(GetDlgItem(h,Prefix))&&IsWindowEnabled(GetDlgItem(h,Suffix)),"Before enables only known end");generateCase("before","aAm1\nbAm1\n");
 mode(1);require(IsWindowEnabled(GetDlgItem(h,Prefix))&&!IsWindowEnabled(GetDlgItem(h,Suffix)),"After enables only known start");generateCase("after","Am1a\nAm1b\n");
 mode(2);field(Prefix,L"Am");field(Suffix,L"1");generateCase("middle","Ama1\nAmb1\n");
 field(Prefix,L"Changed");field(Suffix,L"Changed");click(Resume);wait(L"Complete.");{std::ifstream file(placementOutputs.back(),std::ios::binary);std::string data((std::istreambuf_iterator<char>(file)),{});require(data=="Ama1\nAmb1\n","Resume ignores changed known parts");}
 click(KnownEnabled);field(Min,L"");field(Max,L"1");generateCase("inactive-known","a\nb\n");
 field(Input,L"");field(Symbol,L"!@!");generateCase("symbols-only","!\n@\n");field(Symbol,L"");field(Emoji,L"😀 😀");generateCase("emoji-only",u8"😀\n");field(Emoji,L"");
 click(KnownEnabled);mode(1);field(Prefix,L"1245876454");field(Suffix,L"ignored");field(Min,L"");field(Max,L"");SendMessageW(GetDlgItem(h,Digit),BM_SETCHECK,BST_CHECKED,0);generateCase("fixed-only","1245876454\n");
 click(Resume);wait(L"Complete.");require(get(GetDlgItem(h,Status)).find(L"Written 1 lines")!=std::wstring::npos,"Completed fixed-only resume never duplicates");
 SendMessageW(GetDlgItem(h,Digit),BM_SETCHECK,BST_UNCHECKED,0);field(Prefix,L"آ😀");generateCase("unicode-fixed",u8"آ😀\n");
 click(KnownEnabled);field(Input,L"ab12");field(Min,L"");field(Max,L"4");
 if(argc>1){click(Preview);std::cout<<"SCREENSHOT_READY\n"<<std::flush;std::this_thread::sleep_for(std::chrono::seconds(15));}
 for(const auto& path:placementOutputs){std::filesystem::remove(path);std::filesystem::remove(path.wstring()+L".pgstate");}
 PostMessageW(h,WM_CLOSE,0,0);require(WaitForSingleObject(process.hProcess,10000)==WAIT_OBJECT_0,"GUI closes");CloseHandle(process.hThread);CloseHandle(process.hProcess);for(auto& p:{out,large,wordOutput,symbolOutput,wordSymbolOutput,extraOutput,limitOutput,customOutput}){std::filesystem::remove(p);std::filesystem::remove(p.wstring()+L".pgstate");}std::cout<<"PASS: GUI optional manual symbols and persisted alphabet, later-rank preview, unique words and final lengths, large-space and default-alphabet preview, Unicode generation, stop, resume, exact count, clean close\n";return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<"\n";if(process.hProcess)TerminateProcess(process.hProcess,1);return 1;}}
