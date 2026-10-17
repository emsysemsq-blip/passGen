#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "engine.hpp"
#include "checkpoint_retry.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>
static void check(bool b,const char* message){if(!b)throw std::runtime_error(message);}
static HANDLE lockCheckpoint(const std::filesystem::path& path){
 auto checkpoint=path;checkpoint+=L".pgstate";
 HANDLE h=CreateFileW(checkpoint.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
 check(h!=INVALID_HANDLE_VALUE,"Could not acquire test checkpoint lock");return h;
}
static std::string read(const std::filesystem::path& p){std::ifstream f(p,std::ios::binary);return std::string((std::istreambuf_iterator<char>(f)),{});}
int main(){namespace fs=std::filesystem;fs::path dir=fs::temp_directory_path()/("passgen-lock-"+std::to_string(GetTickCount64()));fs::create_directories(dir);HANDLE locked=INVALID_HANDLE_VALUE;std::thread unlocker;
 try{pg::Config c;c.tokens={"a","b"};c.max_length=8;
 auto transient=dir/"transient.txt";pg::Progress p;auto start=std::chrono::steady_clock::now();
 pg::run(c,transient.u8string(),false,p,[&](const pg::Config&){locked=lockCheckpoint(transient);HANDLE release=locked;unlocker=std::thread([release]{Sleep(250);CloseHandle(release);});});
 unlocker.join();locked=INVALID_HANDLE_VALUE;
 check(p.scanned.load()==pg::total(c)&&p.written.load()==510,"Temporary lock did not recover to exact completion");
 check(std::chrono::steady_clock::now()-start>=std::chrono::milliseconds(150),"Temporary lock was not exercised");
 auto persistent=dir/"persistent.txt";pg::Progress blocked;bool failed=false;DWORD expectedError=0;std::string observed;
 try{pg::run(c,persistent.u8string(),false,blocked,[&](const pg::Config&){locked=lockCheckpoint(persistent);
  auto destination=persistent;destination+=L".pgstate";auto probe=destination;probe+=L".probe";
  {std::ofstream f(probe,std::ios::binary);f<<"locked replacement probe";}
  BOOL moved=MoveFileExW(probe.c_str(),destination.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH);
  expectedError=moved?0:GetLastError();
  check(!moved&&pg::transientCheckpointError(expectedError),"Reference Windows replacement did not encounter the intended sharing lock");
  fs::remove(probe);
 });}
 catch(const std::exception& e){observed=e.what();std::string code="Windows error "+std::to_string(expectedError);failed=observed.find("Checkpoint replace failed")!=std::string::npos&&(observed.find(code+":")!=std::string::npos||observed.find(code+")")!=std::string::npos);}
 if(!failed)std::cerr<<"Observed: "<<observed<<" | Reference Windows code: "<<expectedError<<"\n";
 check(failed,"Persistent sharing violation must preserve the actual Windows error code");
 check(blocked.scanned.load()==0,"Uncommitted output must not advance confirmed progress");
 auto checkpoint=persistent;checkpoint+=L".pgstate";std::string committed=read(checkpoint);
 check(committed.find("PASSGEN_4\n0 0 0 ")==0,"Previous committed checkpoint was lost");
 check(!read(persistent).empty(),"Failure scenario did not write an uncommitted output tail");
 CloseHandle(locked);locked=INVALID_HANDLE_VALUE;
 pg::Progress resumed;pg::run({},persistent.u8string(),true,resumed);
 check(resumed.written.load()==510&&read(persistent)==read(transient),"Resume after checkpoint lock must regenerate without gaps or duplicates");
 fs::remove_all(dir);std::cout<<"PASS: real temporary Windows checkpoint lock, persistent lock diagnostics, preserved checkpoint and exact resume\n";return 0;
 }catch(const std::exception& e){if(unlocker.joinable()){unlocker.join();locked=INVALID_HANDLE_VALUE;}if(locked!=INVALID_HANDLE_VALUE)CloseHandle(locked);std::cerr<<"FAIL: "<<e.what()<<" | artifacts "<<dir<<'\n';return 1;}}
