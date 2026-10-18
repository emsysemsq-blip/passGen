#pragma once
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <thread>
#include <stdexcept>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <time.h>
#endif

namespace pg {
// Cooperative pacing of worker CPU time. This does not limit other processes,
// OS caches, setup, or the coordinator that writes and commits output.
class CpuBudget {
 double fraction;
 const std::atomic<bool>& stop;
 uint64_t initial_cpu;
 std::chrono::steady_clock::time_point initial_wall;
 static uint64_t cpu_nanoseconds(){
#ifdef _WIN32
  FILETIME created,exited,kernel,user;
  if(!GetThreadTimes(GetCurrentThread(),&created,&exited,&kernel,&user))
   throw std::runtime_error("Cannot measure worker CPU time");
  ULARGE_INTEGER k{},u{};
  k.LowPart=kernel.dwLowDateTime;k.HighPart=kernel.dwHighDateTime;
  u.LowPart=user.dwLowDateTime;u.HighPart=user.dwHighDateTime;
  return (k.QuadPart+u.QuadPart)*100;
#else
  timespec value{};
  if(clock_gettime(CLOCK_THREAD_CPUTIME_ID,&value)!=0)
   throw std::runtime_error("Cannot measure worker CPU time");
  return uint64_t(value.tv_sec)*1000000000ull+uint64_t(value.tv_nsec);
#endif
 }
 static void pause(double seconds){
#ifdef _WIN32
  // Reuse one high-resolution timer per persistent worker (Windows 10+).
  // Older systems retain the standard sleep fallback.
  struct Timer {
   HANDLE handle=CreateWaitableTimerExW(nullptr,nullptr,0x00000002,TIMER_MODIFY_STATE|SYNCHRONIZE);
   ~Timer(){if(handle)CloseHandle(handle);}
  };
  thread_local Timer timer;
  if(timer.handle){
   LARGE_INTEGER due{};due.QuadPart=-std::max<LONGLONG>(1,LONGLONG(seconds*10000000));
   if(SetWaitableTimer(timer.handle,&due,0,nullptr,nullptr,FALSE)&&WaitForSingleObject(timer.handle,100)==WAIT_OBJECT_0)return;
  }
#endif
  std::this_thread::sleep_for(std::chrono::duration<double>(seconds));
 }
public:
 CpuBudget(double share,const std::atomic<bool>& stopped):fraction(share),stop(stopped),
  initial_cpu(share<1?cpu_nanoseconds():0),initial_wall(std::chrono::steady_clock::now()){}
 void pace(){
  if(fraction>=1||stop.load())return;
  const double required=double(cpu_nanoseconds()-initial_cpu)/1e9/fraction;
  while(!stop.load()){
   double elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-initial_wall).count();
   if(elapsed>=required)return;
   pause(std::min(0.01,required-elapsed));
  }
 }
};
}
