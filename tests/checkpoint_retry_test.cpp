#include "checkpoint_retry.hpp"
#include <iostream>
#include <stdexcept>
#include <vector>
static void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(){try{
 unsigned calls=0;std::vector<unsigned> waits;
 auto error=pg::retryCheckpointReplace([&]{return ++calls<4?32u:0u;},[&](unsigned ms){waits.push_back(ms);});
 check(error==0&&calls==4&&waits==std::vector<unsigned>({25,50,100}),"Transient sharing violation should recover with backoff");
 calls=0;waits.clear();error=pg::retryCheckpointReplace([&]{++calls;return 5u;},[&](unsigned ms){waits.push_back(ms);});
 check(error==5&&calls==8&&waits.size()==7,"Access denied retries must be bounded");
 calls=0;waits.clear();error=pg::retryCheckpointReplace([&]{++calls;return 112u;},[&](unsigned ms){waits.push_back(ms);});
 check(error==112&&calls==1&&waits.empty(),"Disk-full failure should report immediately");
 calls=0;error=pg::retryCheckpointReplace([&]{++calls;return 0u;},[](unsigned){throw std::runtime_error("Unexpected wait");});
 check(error==0&&calls==1,"Uncontended checkpoint should not wait");
 std::cout<<"PASS: checkpoint backoff, bounded retries, immediate permanent-error reporting, zero-delay success\n";return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
