#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commdlg.h>
#include <commctrl.h>
#include <string>
#include <sstream>
#include <thread>
#include <mutex>
#include <atomic>
#include <algorithm>
#include <chrono>
#include <iomanip>
#include "engine.hpp"
#include "version.hpp"
#include "ui_ids.hpp"
namespace {
using namespace pg::ui;
HWND window;HFONT font;pg::Progress progress;std::thread worker;std::atomic<bool> finished{false};bool running=false,closing=false;std::mutex statusMutex;std::wstring result,jobResources;pg::SharedIndex initial;std::atomic<uint64_t> initialBytes{0};std::atomic<bool> settingsLoaded{false};std::atomic<int64_t> elapsedNs{0};std::chrono::steady_clock::time_point began;
std::wstring wide(const std::string& s){if(s.empty())return {};int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),(int)s.size(),nullptr,0);if(!n)throw std::runtime_error("Invalid UTF-8");std::wstring w(n,0);MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),(int)s.size(),w.data(),n);return w;}
std::string utf8(const std::wstring& s){if(s.empty())return {};int n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,s.data(),(int)s.size(),nullptr,0,nullptr,nullptr);if(!n)throw std::runtime_error("Invalid Unicode text");std::string b(n,0);WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,s.data(),(int)s.size(),b.data(),n,nullptr,nullptr);return b;}
HWND item(int id){return GetDlgItem(window,id);}std::wstring text(int id){int n=GetWindowTextLengthW(item(id));std::wstring s(n+1,0);GetWindowTextW(item(id),s.data(),n+1);s.resize(n);return s;}
std::map<std::string,unsigned> tokenCaps;std::vector<std::string> shownTokens;bool refreshingCaps=false;std::wstring lastPoolSignature;
void set(int id,const std::wstring& s){SetWindowTextW(item(id),s.c_str());}
bool checked(int id){return SendMessageW(item(id),BM_GETCHECK,0,0)==BST_CHECKED;}
unsigned number(int id,unsigned fallback){auto s=text(id);if(s.empty())return fallback;if(s.find_first_not_of(L"0123456789")!=std::wstring::npos)throw std::runtime_error("Lengths and workers must be positive whole numbers");auto n=std::stoul(s);if(n>128)throw std::runtime_error("Numeric setting is too large");return (unsigned)n;}
pg::Config inputPool(){pg::Config c;c.direct_combinations=checked(Words);c.tokens=pg::tokenize(utf8(text(Input)),checked(Words));pg::add_symbols(c,utf8(text(Symbol)));pg::add_emojis(c,utf8(text(Emoji)));if(checked(Persian))pg::add_persian(c);if(checked(ShiftLetters))pg::add_shift_letters(c);for(const auto& token:c.tokens){auto cap=tokenCaps.find(token);if(cap!=tokenCaps.end())c.token_limits.emplace(*cap);}return c;}
void showCapEditor(){
 int row=ListView_GetNextItem(item(TokenList),-1,LVNI_SELECTED);ShowWindow(item(TokenCap),SW_HIDE);
 if(row<0||size_t(row)>=shownTokens.size())return;
 RECT cell{};if(!ListView_GetSubItemRect(item(TokenList),row,1,LVIR_BOUNDS,&cell))return;
 RECT client{},header{};GetClientRect(item(TokenList),&client);GetWindowRect(ListView_GetHeader(item(TokenList)),&header);if(cell.top<header.bottom-header.top||cell.bottom>client.bottom)return;
 MapWindowPoints(item(TokenList),window,reinterpret_cast<POINT*>(&cell),2);
 auto found=tokenCaps.find(shownTokens[row]);SendMessageW(item(TokenCap),CB_SETCURSEL,found==tokenCaps.end()?0:found->second,0);
 SetWindowPos(item(TokenCap),HWND_TOP,cell.left,cell.top,cell.right-cell.left,240,SWP_SHOWWINDOW);
}
std::wstring poolSignature(){return text(Input)+L"\x0001"+text(Symbol)+L"\x0001"+text(Emoji)+L"\x0001"+std::to_wstring(checked(Words))+std::to_wstring(checked(Persian))+std::to_wstring(checked(ShiftLetters));}
void clearCapsOnError(const std::exception& error){refreshingCaps=true;ShowWindow(item(TokenCap),SW_HIDE);ListView_DeleteAllItems(item(TokenList));shownTokens.clear();refreshingCaps=false;set(Sample,L"Input needs attention: "+wide(error.what())+L"\r\nFor a list of complete words or numbers, enable Whole words. No old token rows are kept.");}
void refreshCaps(){
 auto pool=inputPool();std::string selected;int old=ListView_GetNextItem(item(TokenList),-1,LVNI_SELECTED);if(old>=0&&size_t(old)<shownTokens.size())selected=shownTokens[old];
 refreshingCaps=true;ShowWindow(item(TokenCap),SW_HIDE);SendMessageW(item(TokenList),WM_SETREDRAW,FALSE,0);ListView_DeleteAllItems(item(TokenList));shownTokens=std::move(pool.tokens);
 int select=-1;
 for(size_t i=0;i<shownTokens.size();++i){auto token=wide(shownTokens[i]);LVITEMW entry{};entry.mask=LVIF_TEXT;entry.iItem=int(i);entry.pszText=token.data();ListView_InsertItem(item(TokenList),&entry);auto cap=tokenCaps.find(shownTokens[i]);auto value=cap==tokenCaps.end()?std::wstring(L"Always"):std::to_wstring(cap->second);ListView_SetItemText(item(TokenList),int(i),1,value.data());if(shownTokens[i]==selected)select=int(i);}
 if(select>=0){ListView_SetItemState(item(TokenList),select,LVIS_SELECTED|LVIS_FOCUSED,LVIS_SELECTED|LVIS_FOCUSED);ListView_EnsureVisible(item(TokenList),select,FALSE);}
 SendMessageW(item(TokenList),WM_SETREDRAW,TRUE,0);InvalidateRect(item(TokenList),nullptr,TRUE);refreshingCaps=false;lastPoolSignature=poolSignature();showCapEditor();
}
pg::Config config(){pg::Config c=inputPool();auto compute=SendMessageW(item(Compute),CB_GETCURSEL,0,0);c.compute_mode=compute==1?pg::ComputeMode::Cpu:compute==2?pg::ComputeMode::GpuPreferred:pg::ComputeMode::Automatic;c.resource_percent=checked(Resource70)?70:0;c.character_lengths=!checked(TokenLengths);c.min_length=number(Min,1);c.threads=number(Threads,1);c.prefix=utf8(text(Prefix));c.suffix=utf8(text(Suffix));c.lower=checked(Lower);c.upper=checked(Upper);c.digit=checked(Digit);c.no_repeat=checked(Repeat);c.max_length=number(Max,pg::default_max_length(c));pg::total(c);return c;}
void error(const std::exception& e){MessageBoxW(window,wide(e.what()).c_str(),L"PassGen - check settings",MB_OK|MB_ICONERROR);}
void control(const wchar_t* cls,const wchar_t* label,int id,int x,int y,int w,int h,DWORD style=0){HWND hnd=CreateWindowExW(wcscmp(cls,L"EDIT")==0?WS_EX_CLIENTEDGE:0,cls,label,WS_CHILD|WS_VISIBLE|style,x,y,w,h,window,(HMENU)(INT_PTR)id,GetModuleHandleW(nullptr),nullptr);SendMessageW(hnd,WM_SETFONT,(WPARAM)font,TRUE);}
void label(const wchar_t* s,int x,int y,int w){control(L"STATIC",s,0,x,y,w,22);}
void enable(bool on){for(int id=Input;id<=Resume;++id)if(item(id))EnableWindow(item(id),on);for(int id=TokenLengths;id<=Resource70;++id)EnableWindow(item(id),on);EnableWindow(item(Compute),on);EnableWindow(item(TokenList),on);EnableWindow(item(TokenCap),on);EnableWindow(item(Threads),on&&!checked(Resource70));EnableWindow(item(Stop),!on);}
void showResources(){
 auto resources=pg::detect_resources(utf8(text(Output)));auto plan=pg::plan_resources(resources);
 std::wostringstream info;info<<L"CPU "<<std::fixed<<std::setprecision(1)<<resources.cpu_capacity<<L" | RAM free "<<double(resources.memory_available_bytes)/(1ull<<30)<<L" GiB | Disk free "<<double(resources.disk_available_bytes)/(1ull<<30)<<L" GiB";
 if(checked(Resource70)){info<<L" | Auto 70%: "<<plan.workers<<L" workers";set(Threads,std::to_wstring(plan.workers));}else info<<L" | Manual";
 set(ResourceInfo,info.str());
}
void showPreview(){refreshCaps();showResources();auto c=config();auto count=pg::total(c);size_t largest=0;for(auto& t:c.tokens)largest=std::max(largest,t.size());pg::Index upper=count;upper*=uint32_t(largest*c.max_length+c.prefix.size()+c.suffix.size()+1);auto sizeText=upper.str();std::wstring sizeLabel=sizeText.size()>18?wide(sizeText.substr(0,1)+"."+sizeText.substr(1,2)+"e+"+std::to_string(sizeText.size()-1)):wide(sizeText);std::wostringstream summary;summary<<L"Candidates: "<<wide(count.str())<<L" | Length: "<<c.min_length<<L".."<<c.max_length<<(c.character_lengths?L" final characters | Output upper bound: ":L" tokens | Output upper bound: ")<<sizeLabel<<L" bytes\r\n";summary<<L"Rules apply to the complete line (ASCII classes). Representative samples, not an exhaustive search:\r\n";
 auto examples=pg::preview(c);
 for(const auto& example:examples.samples)summary<<wide(example)<<L"\r\n";
 if(examples.samples.empty())summary<<(count?L"No match among the sampled ranks; generation can still find matches later.\r\n":L"No candidate fits the selected length bounds.\r\n");
 summary<<L"Sampled "<<examples.inspected<<L" ranks. This preview does not limit generation.\r\n";
 set(Sample,summary.str());}
void browse(bool resume){wchar_t name[32768]={};auto old=text(Output);if(old.size()<32768)std::copy(old.begin(),old.end(),name);OPENFILENAMEW o{};o.lStructSize=sizeof(o);o.hwndOwner=window;o.lpstrFile=name;o.nMaxFile=32768;o.lpstrFilter=L"Wordlist (*.txt)\0*.txt\0All files\0*.*\0";o.lpstrDefExt=L"txt";o.Flags=OFN_EXPLORER|OFN_NOCHANGEDIR|OFN_PATHMUSTEXIST|(resume?OFN_FILEMUSTEXIST:0);if(resume?GetOpenFileNameW(&o):GetSaveFileNameW(&o))set(Output,name);}
void start(bool resume){if(running)return;pg::Config c;if(!resume){c=config();showPreview();}if(text(Output).empty())browse(resume);if(text(Output).empty())return;auto output=utf8(text(Output));if(!resume&&pg::total(c)>10000000){if(MessageBoxW(window,L"This job scans over 10 million candidates and may create a large file. Check the preview estimate and free disk space. Continue?",L"Large output",MB_YESNO|MB_ICONQUESTION)!=IDYES)return;}
 progress.direct_combinations=false;progress.constructed=0;progress.stop=false;progress.scanned=0;progress.written=0;progress.bytes=0;progress.total=0;progress.disk_limit=false;progress.compute_status.set("");progress.gpu_available=false;progress.gpu_used=false;progress.gpu_scanned=0;initial=0;initialBytes=0;settingsLoaded=false;elapsedNs=0;finished=false;running=true;began=std::chrono::steady_clock::now();enable(false);set(Status,resume?L"Loading checkpoint...":L"Starting... Checking available compute resources.");
 worker=std::thread([c,output,resume]{std::wstring message;try{pg::run(c,output,resume,progress,[](const pg::Config& saved){
 initial=progress.scanned.load();initialBytes=progress.bytes.load();
 std::wostringstream info;info<<L"Active policy: ";if(saved.resource_percent)info<<saved.resource_percent<<L"% auto";else info<<L"manual (saved settings)";
 info<<L" | "<<progress.active_workers.load()<<L" workers | buffer/worker "<<std::fixed<<std::setprecision(1)<<double(progress.buffer_bytes.load())/1048576<<L" MiB";
 if(saved.resource_percent)info<<L" | disk budget "<<double(progress.disk_budget_bytes.load())/(1ull<<30)<<L" GiB";
 {std::lock_guard<std::mutex> lock(statusMutex);jobResources=info.str();}settingsLoaded=true;
});message=progress.scanned.load()==progress.total.load()?L"Complete. Output and checkpoint saved.":progress.disk_limit?L"Stopped safely at disk budget. Resume recalculates free-space budget.":L"Stopped safely. Use Resume to continue the same output.";}catch(const std::exception& e){message=L"Failed: "+wide(e.what())+L". Keep the output and .pgstate together; resume the last checkpoint after fixing the error.";}catch(...){message=L"Unexpected failure. Keep the output and checkpoint for recovery.";}{std::lock_guard<std::mutex> lock(statusMutex);result=message;}elapsedNs=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-began).count();finished=true;});
}
LRESULT CALLBACK proc(HWND h,UINT msg,WPARAM wp,LPARAM lp){switch(msg){case WM_CREATE:{window=h;NONCLIENTMETRICSW metrics{};metrics.cbSize=sizeof(metrics);SystemParametersInfoW(SPI_GETNONCLIENTMETRICS,sizeof(metrics),&metrics,0);font=CreateFontIndirectW(&metrics.lfMessageFont);
 label(L"PassGen " PASSGEN_VERSION_W L" | Native Windows wordlist generator",20,16,740);
 label(L"Input words or characters (whole-word mode keeps each entry intact):",20,49,740);
 control(L"EDIT",L"",Input,20,74,350,101,WS_TABSTOP|ES_MULTILINE|ES_WANTRETURN|ES_AUTOVSCROLL|WS_VSCROLL);
 control(WC_LISTVIEWW,L"",TokenList,400,74,360,101,WS_TABSTOP|LVS_REPORT|LVS_SINGLESEL|LVS_SHOWSELALWAYS);
 ListView_SetExtendedListViewStyle(item(TokenList),LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER);
 LVCOLUMNW column{};column.mask=LVCF_TEXT|LVCF_WIDTH;column.pszText=const_cast<wchar_t*>(L"Input token");column.cx=225;ListView_InsertColumn(item(TokenList),0,&column);column.pszText=const_cast<wchar_t*>(L"Maximum / row");column.cx=110;ListView_InsertColumn(item(TokenList),1,&column);
 control(L"COMBOBOX",L"",TokenCap,0,0,110,240,WS_TABSTOP|CBS_DROPDOWNLIST|WS_VSCROLL);SendMessageW(item(TokenCap),CB_ADDSTRING,0,(LPARAM)L"Always");for(unsigned cap=1;cap<=10;++cap){auto name=std::to_wstring(cap);SendMessageW(item(TokenCap),CB_ADDSTRING,0,(LPARAM)name.c_str());}ShowWindow(item(TokenCap),SW_HIDE);
 control(L"BUTTON",L"Whole words: combine complete entries (spaces or new lines separate words)",Words,20,182,740,24,WS_TABSTOP|BS_AUTOCHECKBOX);SendMessageW(item(Words),BM_SETCHECK,BST_CHECKED,0);
 label(L"Min length",20,217,100);control(L"EDIT",L"",Min,115,214,80,26,WS_TABSTOP|ES_NUMBER);
 label(L"Max length",215,217,100);control(L"EDIT",L"",Max,310,214,80,26,WS_TABSTOP|ES_NUMBER);
 label(L"Workers (1..32)",415,217,135);std::wstring n=std::to_wstring(std::min(8u,std::max(1u,std::thread::hardware_concurrency())));control(L"EDIT",n.c_str(),Threads,555,214,80,26,WS_TABSTOP|ES_NUMBER);
 control(L"BUTTON",L"Auto 70%",Resource70,650,214,110,26,WS_TABSTOP|BS_AUTOCHECKBOX);SendMessageW(item(Resource70),BM_SETCHECK,BST_CHECKED,0);EnableWindow(item(Threads),FALSE);
 control(L"STATIC",L"Detecting resources...",ResourceInfo,20,247,740,22);
 label(L"Prefix",20,282,75);control(L"EDIT",L"",Prefix,100,278,280,26,WS_TABSTOP|ES_AUTOHSCROLL);label(L"Suffix",400,282,70);control(L"EDIT",L"",Suffix,475,278,285,26,WS_TABSTOP|ES_AUTOHSCROLL);
 control(L"BUTTON",L"Require a-z",Lower,20,318,140,24,WS_TABSTOP|BS_AUTOCHECKBOX);control(L"BUTTON",L"Require A-Z",Upper,175,318,140,24,WS_TABSTOP|BS_AUTOCHECKBOX);control(L"BUTTON",L"Require 0-9",Digit,330,318,140,24,WS_TABSTOP|BS_AUTOCHECKBOX);label(L"Symbols (manual)",485,318,140);control(L"EDIT",L"",Symbol,630,314,130,26,WS_TABSTOP|ES_AUTOHSCROLL);
 control(L"BUTTON",L"Reject adjacent identical tokens",Repeat,20,351,375,24,WS_TABSTOP|BS_AUTOCHECKBOX);
 control(L"BUTTON",L"Length counts tokens instead",TokenLengths,420,351,340,24,WS_TABSTOP|BS_AUTOCHECKBOX);
 label(L"Emoji (manual; separate entries with spaces)",20,386,370);control(L"EDIT",L"",Emoji,400,382,360,26,WS_TABSTOP|ES_AUTOHSCROLL);
 control(L"BUTTON",L"Add Persian letters (including آ)",Persian,20,419,375,24,WS_TABSTOP|BS_AUTOCHECKBOX);
 control(L"BUTTON",L"Shift + letter: Windows Persian",ShiftLetters,400,419,360,24,WS_TABSTOP|BS_AUTOCHECKBOX);
 label(L"Compute",20,457,80);control(L"COMBOBOX",L"",Compute,100,453,300,120,WS_TABSTOP|CBS_DROPDOWNLIST|WS_VSCROLL);for(auto option:{L"Auto: CPU + compatible GPU",L"CPU only",L"Prefer GPU + CPU fallback"})SendMessageW(item(Compute),CB_ADDSTRING,0,(LPARAM)option);SendMessageW(item(Compute),CB_SETCURSEL,0,0);label(L"Auto/GPU: checks driver at start",420,457,340);
 label(L"Output file (UTF-8, one result per line; new jobs never overwrite files):",20,493,740);control(L"EDIT",L"",Output,20,520,625,26,WS_TABSTOP|ES_AUTOHSCROLL);control(L"BUTTON",L"Browse...",Browse,660,518,100,30,WS_TABSTOP);
 control(L"BUTTON",L"Preview",Preview,20,563,130,34,WS_TABSTOP);control(L"BUTTON",L"Generate",Start,165,563,150,34,WS_TABSTOP|BS_DEFPUSHBUTTON);control(L"BUTTON",L"Resume output",Resume,330,563,170,34,WS_TABSTOP);control(L"BUTTON",L"Stop && save",Stop,515,563,160,34,WS_TABSTOP);EnableWindow(item(Stop),FALSE);
 control(L"EDIT",L"Select an input row above to set Always or a maximum of 1..10.\r\nChoose input and click Preview. All selected lengths are scanned; output lines stay unique.\r\nKeep output.txt and output.txt.pgstate together for recovery.\r\nSymbols: type only the desired characters, e.g. !@#. They are optional in each password; blank adds none.",Sample,20,612,740,95,ES_READONLY|ES_MULTILINE|ES_AUTOVSCROLL|WS_VSCROLL);
 control(PROGRESS_CLASSW,L"",Bar,20,722,740,22);SendMessageW(item(Bar),PBM_SETRANGE32,0,1000);control(L"STATIC",L"Ready. Duplicate output lines are always excluded. Auto checks GPU at generation start.",Status,20,758,740,70);
 refreshCaps();showResources();SetTimer(h,1,250,nullptr);return 0;}
 case WM_NOTIFY:{auto header=reinterpret_cast<NMHDR*>(lp);if(header->idFrom==TokenList&&!refreshingCaps){if(header->code==LVN_ITEMCHANGED){auto change=reinterpret_cast<NMLISTVIEW*>(lp);if(change->iItem>=0&&(change->uNewState&LVIS_SELECTED)&&!(change->uOldState&LVIS_SELECTED))ListView_EnsureVisible(item(TokenList),change->iItem,FALSE);}if(header->code==LVN_BEGINSCROLL)ShowWindow(item(TokenCap),SW_HIDE);else if(header->code==LVN_ITEMCHANGED||header->code==NM_CLICK||header->code==LVN_ENDSCROLL)showCapEditor();}return 0;}
 case WM_COMMAND:try{
 if(LOWORD(wp)==TokenCap&&HIWORD(wp)==CBN_SELCHANGE){int row=ListView_GetNextItem(item(TokenList),-1,LVNI_SELECTED);auto cap=SendMessageW(item(TokenCap),CB_GETCURSEL,0,0);if(row>=0&&size_t(row)<shownTokens.size()&&cap>=0&&cap<=10){if(cap)tokenCaps[shownTokens[row]]=unsigned(cap);else tokenCaps.erase(shownTokens[row]);auto value=cap?std::to_wstring(cap):std::wstring(L"Always");ListView_SetItemText(item(TokenList),row,1,value.data());}return 0;}
 if((HIWORD(wp)==EN_CHANGE&&(LOWORD(wp)==Input||LOWORD(wp)==Symbol||LOWORD(wp)==Emoji))||(HIWORD(wp)==BN_CLICKED&&(LOWORD(wp)==Words||LOWORD(wp)==Persian||LOWORD(wp)==ShiftLetters)))SetTimer(h,2,150,nullptr);
 switch(LOWORD(wp)){case Resource70:EnableWindow(item(Threads),!checked(Resource70));showResources();break;case Browse:browse(false);showResources();break;case Preview:showPreview();break;case Start:start(false);break;case Resume:if(text(Output).empty())browse(true);start(true);break;case Stop:progress.stop=true;set(Status,L"Stopping after current batch is safely saved...");break;}}catch(const std::exception& e){error(e);}return 0;
 case WM_TIMER:if(wp==2){KillTimer(h,2);try{refreshCaps();}catch(const std::exception& e){lastPoolSignature=poolSignature();clearCapsOnError(e);}return 0;}if(!running&&poolSignature()!=lastPoolSignature){try{refreshCaps();}catch(const std::exception& e){lastPoolSignature=poolSignature();clearCapsOnError(e);}}if(running){if(settingsLoaded){std::lock_guard<std::mutex> lock(statusMutex);set(ResourceInfo,jobResources);}auto n=progress.scanned.load(),t=progress.total.load();SendMessageW(item(Bar),PBM_SETPOS,t?(WPARAM)(n.fractionOf(t)*1000):0,0);double sec=finished?double(elapsedNs.load())/1e9:std::chrono::duration<double>(std::chrono::steady_clock::now()-began).count();auto first=initial.load();bool measured=settingsLoaded&&n>=first;std::wostringstream s;s<<(progress.direct_combinations?L"Covered ranks ":L"Scanned ")<<wide(n.str())<<L" / "<<wide(t.str())<<L" | Written "<<progress.written.load()<<L" lines | "<<progress.bytes.load()<<L" bytes\r\n"<<std::fixed<<std::setprecision(0)<<(sec>0&&measured?(progress.direct_combinations?double(progress.constructed.load()):(n-first).approximate())/sec:0)<<(progress.direct_combinations?L" constructed combinations/sec | ":L" candidates/sec | ")<<std::setprecision(1)<<(sec>0&&measured?double(progress.bytes.load()-initialBytes.load())/(1048576.0*sec):0)<<L" MiB/sec | GPU scanned "<<progress.gpu_scanned.load();auto computeStatus=wide(progress.compute_status.get());if(computeStatus.size()>100){computeStatus.resize(100);computeStatus+=L"...";}if(!computeStatus.empty())s<<L"\r\n"<<computeStatus;set(Status,s.str());if(finished){worker.join();running=false;enable(true);{std::lock_guard<std::mutex> lock(statusMutex);set(Status,s.str()+L"\r\n"+result);}if(closing)DestroyWindow(h);}}return 0;
 case WM_CLOSE:if(running){if(MessageBoxW(h,L"Stop safely and close? The current batch will be checkpointed.",L"PassGen",MB_YESNO|MB_ICONQUESTION)==IDYES){closing=true;progress.stop=true;}return 0;}DestroyWindow(h);return 0;
 case WM_DESTROY:KillTimer(h,1);if(worker.joinable()){progress.stop=true;worker.join();}DeleteObject(font);PostQuitMessage(0);return 0;
 }return DefWindowProcW(h,msg,wp,lp);}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR,int show){SetProcessDPIAware();INITCOMMONCONTROLSEX init{sizeof(init),ICC_PROGRESS_CLASS|ICC_LISTVIEW_CLASSES};InitCommonControlsEx(&init);WNDCLASSW cls{};cls.lpfnWndProc=proc;cls.hInstance=instance;cls.hCursor=LoadCursorW(nullptr,IDC_ARROW);cls.hIcon=LoadIconW(nullptr,IDI_APPLICATION);cls.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);cls.lpszClassName=L"PassGenWindow";RegisterClassW(&cls);RECT r{0,0,785,847};AdjustWindowRect(&r,WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,FALSE);HWND h=CreateWindowW(cls.lpszClassName,L"PassGen " PASSGEN_VERSION_W,WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,CW_USEDEFAULT,CW_USEDEFAULT,r.right-r.left,r.bottom-r.top,nullptr,nullptr,instance,nullptr);if(!h)return 1;ShowWindow(h,show);MSG msg;while(GetMessageW(&msg,nullptr,0,0)>0){if(!IsDialogMessageW(h,&msg)){TranslateMessage(&msg);DispatchMessageW(&msg);}}return (int)msg.wParam;}
