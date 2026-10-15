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
#include "ui_ids.hpp"
namespace {
using namespace pg::ui;
HWND window;HFONT font;pg::Progress progress;std::thread worker;std::atomic<bool> finished{false};bool running=false,closing=false;std::mutex statusMutex;std::wstring result;uint64_t initial=0;std::chrono::steady_clock::time_point began;
std::wstring wide(const std::string& s){if(s.empty())return {};int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),(int)s.size(),nullptr,0);if(!n)throw std::runtime_error("Invalid UTF-8");std::wstring w(n,0);MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),(int)s.size(),w.data(),n);return w;}
std::string utf8(const std::wstring& s){if(s.empty())return {};int n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,s.data(),(int)s.size(),nullptr,0,nullptr,nullptr);if(!n)throw std::runtime_error("Invalid Unicode text");std::string b(n,0);WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,s.data(),(int)s.size(),b.data(),n,nullptr,nullptr);return b;}
HWND item(int id){return GetDlgItem(window,id);}std::wstring text(int id){int n=GetWindowTextLengthW(item(id));std::wstring s(n+1,0);GetWindowTextW(item(id),s.data(),n+1);s.resize(n);return s;}
void set(int id,const std::wstring& s){SetWindowTextW(item(id),s.c_str());}
bool checked(int id){return SendMessageW(item(id),BM_GETCHECK,0,0)==BST_CHECKED;}
unsigned number(int id,unsigned fallback){auto s=text(id);if(s.empty())return fallback;if(s.find_first_not_of(L"0123456789")!=std::wstring::npos)throw std::runtime_error("Lengths and workers must be positive whole numbers");auto n=std::stoul(s);if(n>128)throw std::runtime_error("Numeric setting is too large");return (unsigned)n;}
pg::Config config(){pg::Config c;c.tokens=pg::tokenize(utf8(text(Input)),checked(Words));c.min_length=number(Min,1);c.max_length=number(Max,(unsigned)c.tokens.size());c.threads=number(Threads,1);c.prefix=utf8(text(Prefix));c.suffix=utf8(text(Suffix));c.lower=checked(Lower);c.upper=checked(Upper);c.digit=checked(Digit);c.symbol=checked(Symbol);c.no_repeat=checked(Repeat);pg::total(c);return c;}
void error(const std::exception& e){MessageBoxW(window,wide(e.what()).c_str(),L"PassGen - check settings",MB_OK|MB_ICONERROR);}
void control(const wchar_t* cls,const wchar_t* label,int id,int x,int y,int w,int h,DWORD style=0){HWND hnd=CreateWindowExW(wcscmp(cls,L"EDIT")==0?WS_EX_CLIENTEDGE:0,cls,label,WS_CHILD|WS_VISIBLE|style,x,y,w,h,window,(HMENU)(INT_PTR)id,GetModuleHandleW(nullptr),nullptr);SendMessageW(hnd,WM_SETFONT,(WPARAM)font,TRUE);}
void label(const wchar_t* s,int x,int y,int w){control(L"STATIC",s,0,x,y,w,22);}
void enable(bool on){for(int id=Input;id<=Resume;++id)if(item(id))EnableWindow(item(id),on);EnableWindow(item(Stop),!on);}
void showPreview(){auto c=config();auto count=pg::total(c);size_t largest=0;for(auto& t:c.tokens)largest=std::max(largest,t.size());long double bytes=(long double)count*(largest*c.max_length+c.prefix.size()+c.suffix.size()+1);std::wostringstream summary;summary<<L"Candidates: "<<count<<L" | Length: "<<c.min_length<<L".."<<c.max_length<<L" tokens | Output upper bound: "<<std::fixed<<std::setprecision(2)<<bytes/(1024*1024*1024)<<L" GiB\r\n";summary<<L"Filters apply to the complete line; class rules cover ASCII. Preview (first matching candidates):\r\n";unsigned shown=0;for(uint64_t i=0;i<std::min<uint64_t>(count,1000)&&shown<12;++i){auto s=pg::candidate(c,i);if(pg::accepts(c,s)){summary<<wide(s)<<L"\r\n";++shown;}}if(!shown)summary<<L"No match in the first 1,000 candidates.\r\n";set(Sample,summary.str());}
void browse(bool resume){wchar_t name[32768]={};auto old=text(Output);if(old.size()<32768)std::copy(old.begin(),old.end(),name);OPENFILENAMEW o{};o.lStructSize=sizeof(o);o.hwndOwner=window;o.lpstrFile=name;o.nMaxFile=32768;o.lpstrFilter=L"Wordlist (*.txt)\0*.txt\0All files\0*.*\0";o.lpstrDefExt=L"txt";o.Flags=OFN_EXPLORER|OFN_NOCHANGEDIR|OFN_PATHMUSTEXIST|(resume?OFN_FILEMUSTEXIST:0);if(resume?GetOpenFileNameW(&o):GetSaveFileNameW(&o))set(Output,name);}
void start(bool resume){if(running)return;pg::Config c;if(!resume){c=config();showPreview();}if(text(Output).empty())browse(resume);if(text(Output).empty())return;auto output=utf8(text(Output));if(!resume&&pg::total(c)>10000000){if(MessageBoxW(window,L"This job scans over 10 million candidates and may create a large file. Check the preview estimate and free disk space. Continue?",L"Large output",MB_YESNO|MB_ICONQUESTION)!=IDYES)return;}
 progress.stop=false;progress.scanned=0;progress.written=0;progress.bytes=0;progress.total=0;initial=0;finished=false;running=true;began=std::chrono::steady_clock::now();enable(false);set(Status,resume?L"Loading checkpoint...":L"Starting...");
 worker=std::thread([c,output,resume]{std::wstring message;try{pg::run(c,output,resume,progress,[](const pg::Config&){});message=progress.scanned==progress.total?L"Complete. Output and checkpoint saved.":L"Stopped safely. Use Resume to continue the same output.";}catch(const std::exception& e){message=L"Failed: "+wide(e.what())+L". Keep the output and .pgstate together; resume the last checkpoint after fixing the error.";}catch(...){message=L"Unexpected failure. Keep the output and checkpoint for recovery.";}{std::lock_guard<std::mutex> lock(statusMutex);result=message;}finished=true;});
}
LRESULT CALLBACK proc(HWND h,UINT msg,WPARAM wp,LPARAM lp){switch(msg){case WM_CREATE:{window=h;NONCLIENTMETRICSW metrics{};metrics.cbSize=sizeof(metrics);SystemParametersInfoW(SPI_GETNONCLIENTMETRICS,sizeof(metrics),&metrics,0);font=CreateFontIndirectW(&metrics.lfMessageFont);
 label(L"PassGen 0.1 | Native Windows wordlist generator",20,16,740);
 label(L"Input characters (Unicode supported), or whitespace-separated words:",20,49,740);
 control(L"EDIT",L"abc123",Input,20,74,740,65,WS_TABSTOP|ES_MULTILINE|ES_AUTOVSCROLL|WS_VSCROLL);
 control(L"BUTTON",L"Word tokens (length counts words; concatenated without separators)",Words,20,146,740,24,WS_TABSTOP|BS_AUTOCHECKBOX);
 label(L"Min length",20,181,100);control(L"EDIT",L"",Min,115,178,80,26,WS_TABSTOP|ES_NUMBER);
 label(L"Max length",215,181,100);control(L"EDIT",L"",Max,310,178,80,26,WS_TABSTOP|ES_NUMBER);
 label(L"Workers (1..32)",415,181,135);std::wstring n=std::to_wstring(std::min(8u,std::max(1u,std::thread::hardware_concurrency())));control(L"EDIT",n.c_str(),Threads,555,178,80,26,WS_TABSTOP|ES_NUMBER);
 label(L"Blank lengths = 1 through number of unique input tokens. Repetition is allowed.",20,211,740);
 label(L"Prefix",20,246,75);control(L"EDIT",L"",Prefix,100,242,280,26,WS_TABSTOP|ES_AUTOHSCROLL);label(L"Suffix",400,246,70);control(L"EDIT",L"",Suffix,475,242,285,26,WS_TABSTOP|ES_AUTOHSCROLL);
 control(L"BUTTON",L"Require a-z",Lower,20,282,140,24,WS_TABSTOP|BS_AUTOCHECKBOX);control(L"BUTTON",L"Require A-Z",Upper,175,282,140,24,WS_TABSTOP|BS_AUTOCHECKBOX);control(L"BUTTON",L"Require 0-9",Digit,330,282,140,24,WS_TABSTOP|BS_AUTOCHECKBOX);control(L"BUTTON",L"Require ASCII symbol",Symbol,485,282,260,24,WS_TABSTOP|BS_AUTOCHECKBOX);
 control(L"BUTTON",L"Reject adjacent identical tokens",Repeat,20,315,400,24,WS_TABSTOP|BS_AUTOCHECKBOX);
 label(L"Output file (UTF-8, one result per line; new jobs never overwrite files):",20,351,740);control(L"EDIT",L"",Output,20,378,625,26,WS_TABSTOP|ES_AUTOHSCROLL);control(L"BUTTON",L"Browse...",Browse,660,376,100,30,WS_TABSTOP);
 control(L"BUTTON",L"Preview",Preview,20,421,130,34,WS_TABSTOP);control(L"BUTTON",L"Generate",Start,165,421,150,34,WS_TABSTOP|BS_DEFPUSHBUTTON);control(L"BUTTON",L"Resume output",Resume,330,421,170,34,WS_TABSTOP);control(L"BUTTON",L"Stop && save",Stop,515,421,160,34,WS_TABSTOP);EnableWindow(item(Stop),FALSE);
 control(L"EDIT",L"Choose input and click Preview. No filters means all indexed combinations.\r\nKeep output.txt and output.txt.pgstate together for recovery.\r\nDifferent word sequences can produce identical text; no global deduplication.",Sample,20,470,740,165,ES_READONLY|ES_MULTILINE|ES_AUTOVSCROLL|WS_VSCROLL);
 control(PROGRESS_CLASSW,L"",Bar,20,650,740,22);SendMessageW(item(Bar),PBM_SETRANGE32,0,1000);control(L"STATIC",L"Ready. Output is generated in bounded, ordered batches.",Status,20,686,740,70);
 SetTimer(h,1,250,nullptr);return 0;}
 case WM_COMMAND:try{switch(LOWORD(wp)){case Browse:browse(false);break;case Preview:showPreview();break;case Start:start(false);break;case Resume:if(text(Output).empty())browse(true);start(true);break;case Stop:progress.stop=true;set(Status,L"Stopping after current batch is safely saved...");break;}}catch(const std::exception& e){error(e);}return 0;
 case WM_TIMER:if(running){auto n=progress.scanned.load(),t=progress.total.load();SendMessageW(item(Bar),PBM_SETPOS,t?(WPARAM)((long double)n/t*1000):0,0);double sec=std::chrono::duration<double>(std::chrono::steady_clock::now()-began).count();if(!initial)initial=n;std::wostringstream s;s<<L"Scanned "<<n<<L" / "<<t<<L" | Written "<<progress.written.load()<<L" lines | "<<progress.bytes.load()<<L" bytes\r\n"<<std::fixed<<std::setprecision(0)<<(sec>0?(n-initial)/sec:0)<<L" candidates/sec";set(Status,s.str());if(finished){worker.join();running=false;enable(true);{std::lock_guard<std::mutex> lock(statusMutex);set(Status,s.str()+L"\r\n"+result);}if(closing)DestroyWindow(h);}}return 0;
 case WM_CLOSE:if(running){if(MessageBoxW(h,L"Stop safely and close? The current batch will be checkpointed.",L"PassGen",MB_YESNO|MB_ICONQUESTION)==IDYES){closing=true;progress.stop=true;}return 0;}DestroyWindow(h);return 0;
 case WM_DESTROY:KillTimer(h,1);if(worker.joinable()){progress.stop=true;worker.join();}DeleteObject(font);PostQuitMessage(0);return 0;
 }return DefWindowProcW(h,msg,wp,lp);}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR,int show){SetProcessDPIAware();INITCOMMONCONTROLSEX init{sizeof(init),ICC_PROGRESS_CLASS};InitCommonControlsEx(&init);WNDCLASSW cls{};cls.lpfnWndProc=proc;cls.hInstance=instance;cls.hCursor=LoadCursorW(nullptr,IDC_ARROW);cls.hIcon=LoadIconW(nullptr,IDI_APPLICATION);cls.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);cls.lpszClassName=L"PassGenWindow";RegisterClassW(&cls);RECT r{0,0,785,775};AdjustWindowRect(&r,WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,FALSE);HWND h=CreateWindowW(cls.lpszClassName,L"PassGen 0.1",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,CW_USEDEFAULT,CW_USEDEFAULT,r.right-r.left,r.bottom-r.top,nullptr,nullptr,instance,nullptr);if(!h)return 1;ShowWindow(h,show);MSG msg;while(GetMessageW(&msg,nullptr,0,0)>0){if(!IsDialogMessageW(h,&msg)){TranslateMessage(&msg);DispatchMessageW(&msg);}}return (int)msg.wParam;}
