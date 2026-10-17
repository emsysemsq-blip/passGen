#pragma once
#include <algorithm>
#include <cstdint>
#include <cmath>
#include <iomanip>
#include <istream>
#include <limits>
#include <mutex>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
namespace pg {
// Nonnegative arbitrary-precision rank. Little-endian base 10^9 limbs.
// Used only for candidate positions/counts; output byte offsets stay bounded by the OS.
class Index {
 static constexpr uint32_t base=1000000000;
 std::vector<uint32_t> limbs;
 void trim(){while(limbs.size()>1&&limbs.back()==0)limbs.pop_back();}
public:
 Index(uint64_t n=0){do{limbs.push_back(uint32_t(n%base));n/=base;}while(n);}
 explicit operator bool()const{return limbs.size()!=1||limbs[0]!=0;}
 bool fits64()const{uint64_t n=0;for(size_t i=limbs.size();i--;){if(n>(UINT64_MAX-limbs[i])/base)return false;n=n*base+limbs[i];}return true;}
 uint64_t u64()const{if(!fits64())throw std::overflow_error("Rank does not fit uint64");uint64_t n=0;for(size_t i=limbs.size();i--;)n=n*base+limbs[i];return n;}
 double fractionOf(const Index& denominator)const{if(!denominator)return 0;if(*this>=denominator)return 1;auto a=str(),b=denominator.str();int exponent=int(a.size())-int(b.size());auto head=[](const std::string& s){size_t n=std::min<size_t>(15,s.size());return std::stod(s.substr(0,n))/std::pow(10.0,int(n)-1);};return head(a)/head(b)*std::pow(10.0,exponent);}
 long double approximate()const{long double n=0;for(size_t i=limbs.size();i--;)n=n*base+limbs[i];return n;}
 std::string str()const{std::ostringstream out;out<<limbs.back();for(size_t i=limbs.size()-1;i--;)out<<std::setw(9)<<std::setfill('0')<<limbs[i];return out.str();}
 friend bool operator<(const Index& a,const Index& b){if(a.limbs.size()!=b.limbs.size())return a.limbs.size()<b.limbs.size();for(size_t i=a.limbs.size();i--;)if(a.limbs[i]!=b.limbs[i])return a.limbs[i]<b.limbs[i];return false;}
 friend bool operator==(const Index& a,const Index& b){return a.limbs==b.limbs;}
 friend bool operator!=(const Index& a,const Index& b){return !(a==b);}
 friend bool operator>(const Index& a,const Index& b){return b<a;}
 friend bool operator<=(const Index& a,const Index& b){return !(b<a);}
 friend bool operator>=(const Index& a,const Index& b){return !(a<b);}
 Index& operator+=(const Index& b){uint64_t carry=0;size_t n=std::max(limbs.size(),b.limbs.size());limbs.resize(n,0);for(size_t i=0;i<n;++i){uint64_t sum=uint64_t(limbs[i])+(i<b.limbs.size()?b.limbs[i]:0)+carry;limbs[i]=uint32_t(sum%base);carry=sum/base;}if(carry)limbs.push_back(uint32_t(carry));return *this;}
 Index& operator-=(const Index& b){if(*this<b)throw std::underflow_error("Negative rank");int64_t borrow=0;for(size_t i=0;i<limbs.size();++i){int64_t n=int64_t(limbs[i])-(i<b.limbs.size()?b.limbs[i]:0)-borrow;borrow=n<0;limbs[i]=uint32_t(n+(borrow?base:0));}trim();return *this;}
 Index& operator*=(uint32_t b){uint64_t carry=0;for(auto& limb:limbs){uint64_t n=uint64_t(limb)*b+carry;limb=uint32_t(n%base);carry=n/base;}while(carry){limbs.push_back(uint32_t(carry%base));carry/=base;}trim();return *this;}
 uint32_t divide(uint32_t divisor){if(!divisor)throw std::invalid_argument("Division by zero");uint64_t remainder=0;for(size_t i=limbs.size();i--;){uint64_t n=remainder*base+limbs[i];limbs[i]=uint32_t(n/divisor);remainder=n%divisor;}trim();return uint32_t(remainder);}
 Index& operator++(){for(auto& limb:limbs){if(++limb<base)return *this;limb=0;}limbs.push_back(1);return *this;}
 friend Index operator+(Index a,const Index& b){return a+=b;}
 friend Index operator-(Index a,const Index& b){return a-=b;}
 friend std::ostream& operator<<(std::ostream& out,const Index& n){return out<<n.str();}
 friend std::istream& operator>>(std::istream& in,Index& n){std::string s;if(!(in>>s))return in;if(s.empty()||s.size()>4096||s.find_first_not_of("0123456789")!=std::string::npos){in.setstate(std::ios::failbit);return in;}Index value;for(char c:s){value*=10;value+=uint64_t(c-'0');}n=std::move(value);return in;}
};
// GUI reads snapshots while the generation thread commits progress.
class SharedIndex {
 mutable std::mutex mutex;
 Index value;
public:
 Index load()const{std::lock_guard<std::mutex> lock(mutex);return value;}
 SharedIndex& operator=(const Index& n){std::lock_guard<std::mutex> lock(mutex);value=n;return *this;}
};
}
