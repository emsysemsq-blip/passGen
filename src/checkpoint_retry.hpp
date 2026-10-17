#pragma once
#include <algorithm>
#include <cstdint>
namespace pg {
// A scanner or another reader may temporarily open the checkpoint without
// delete-sharing. Never remove the committed file to work around this lock.
inline bool transientCheckpointError(uint32_t error){
 return error==5 || error==32 || error==33 || error==1224;
}
template<class Attempt,class Wait>
uint32_t retryCheckpointReplace(Attempt attempt,Wait wait){
 constexpr unsigned attempts=8;
 uint32_t error=0;
 for(unsigned n=0;n<attempts;++n){
  error=attempt();
  if(error==0||!transientCheckpointError(error)||n+1==attempts)return error;
  wait(std::min(400u,25u<<n));
 }
 return error;
}
}
