#include "index.hpp"
#include <iostream>
int main(){pg::Index a,b;uint32_t n;while(std::cin>>a>>b>>n){auto product=a;product*=n;auto quotient=a;auto remainder=quotient.divide(n);std::cout<<(a+b)<<' '<<(a<b?b-a:a-b)<<' '<<product<<' '<<quotient<<' '<<remainder<<' '<<(a<b)<<' '<<a.fits64()<<'\n';}}
