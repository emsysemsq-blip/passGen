#pragma once
#include "engine.hpp"
#include <algorithm>
#include <limits>
#include <unordered_map>
#include <utility>
namespace pg {
// Select one canonical tokenization per output string. Prefix-free alphabets
// (including Unicode scalar characters) need no lookup or stored output history.
class UniqueOutputs {
 struct Node { std::vector<std::pair<unsigned char,size_t>> edges; int token=-1; };
 struct Match { int token; size_t end; };
 struct Choice { int first=-1, second=-1; };
 const Config& config;
 std::vector<Node> trie;
 size_t min_bytes=SIZE_MAX,max_bytes=0;
 bool ambiguous=false;
 unsigned min_tokens,max_tokens;
 size_t affix_chars=0;
 std::vector<size_t> token_chars;
 struct Query {
  const UniqueOutputs& owner;
  const std::string& body;
  std::unordered_map<size_t,std::vector<Match>> matching;
  std::unordered_map<uint64_t,Choice> memo;
  const std::vector<Match>& matches(size_t pos){
   auto found=matching.find(pos);if(found!=matching.end())return found->second;
   std::vector<Match> values;size_t node=0;
   for(size_t i=pos;i<body.size();++i){
    const auto& edges=owner.trie[node].edges;
    auto edge=std::find_if(edges.begin(),edges.end(),[&](auto e){return e.first==(unsigned char)body[i];});
    if(edge==edges.end())break;
    node=edge->second;
    if(owner.trie[node].token>=0)values.push_back({owner.trie[node].token,i+1});
   }
   std::sort(values.begin(),values.end(),[](auto a,auto b){return a.token<b.token;});
   return matching.emplace(pos,std::move(values)).first->second;
  }
  Choice solve(size_t pos,unsigned remaining){
   if(remaining==0||pos>=body.size())return {};
   size_t bytes=body.size()-pos;
   if(bytes<remaining*owner.min_bytes||bytes>remaining*owner.max_bytes)return {};
   uint64_t key=uint64_t(pos)*(owner.config.max_length+1)+remaining;
   auto found=memo.find(key);if(found!=memo.end())return found->second;
   Choice result;
   for(auto match:matches(pos)){
    bool feasible=false;
    if(remaining==1)feasible=match.end==body.size();
    else {
     auto suffix=solve(match.end,remaining-1);
     feasible=suffix.first>=0&&(!owner.config.no_repeat||suffix.first!=match.token||suffix.second>=0);
    }
    if(!feasible)continue;
    if(result.first<0){result.first=match.token;if(!owner.config.no_repeat)break;}
    else {result.second=match.token;break;}
   }
   memo.emplace(key,result);return result;
  }
 };
public:
 explicit UniqueOutputs(const Config& c,unsigned minimum,unsigned maximum):config(c),min_tokens(minimum),max_tokens(maximum){
  if(c.character_lengths){affix_chars=characters(c.prefix)+characters(c.suffix);for(const auto& token:c.tokens)token_chars.push_back(characters(token));}
  std::vector<std::string> sorted=c.tokens;std::sort(sorted.begin(),sorted.end());
  for(size_t i=1;i<sorted.size();++i)if(sorted[i].compare(0,sorted[i-1].size(),sorted[i-1])==0){ambiguous=true;break;}
  if(!ambiguous)return;
  trie.emplace_back();
  for(size_t token=0;token<c.tokens.size();++token){size_t node=0;const auto& text=c.tokens[token];min_bytes=std::min(min_bytes,text.size());max_bytes=std::max(max_bytes,text.size());
   for(unsigned char byte:text){auto& edges=trie[node].edges;auto edge=std::find_if(edges.begin(),edges.end(),[&](auto e){return e.first==byte;});
    if(edge!=edges.end())node=edge->second;
    else {size_t next=trie.size();edges.push_back({byte,next});trie.emplace_back();node=next;}}
   if(trie[node].token<0)trie[node].token=int(token);
  }
 }
 bool needsCanonicalization()const{return ambiguous;}
 unsigned minimumTokens()const{return min_tokens;}
 unsigned maximumTokens()const{return max_tokens;}
 size_t affixCharacters()const{return affix_chars;}
 size_t tokenCharacters(size_t token)const{return config.character_lengths?token_chars[token]:0;}
 bool canonical(const std::string& body,const size_t* digits,unsigned length)const{
  if(!ambiguous)return true;
  Query query{*this,body,{}, {}};
  // Enumeration is ordered by token count, then input-token order.
  for(unsigned n=min_tokens;n<length;++n)if(query.solve(0,n).first>=0)return false;
  size_t pos=0;int previous=-1;
  for(unsigned n=length;n>0;--n){auto choices=query.solve(pos,n);int selected=choices.first;
   if(config.no_repeat&&selected==previous)selected=choices.second;
   if(selected<0||size_t(selected)!=digits[length-n])return false;
   pos+=config.tokens[selected].size();previous=selected;
  }
  return pos==body.size();
 }
};
}
