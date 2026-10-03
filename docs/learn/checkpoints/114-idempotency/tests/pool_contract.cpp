#include "renderer/handle_pool.h"
#include <cstdio>
#include <cstdlib>
#include <new>
#include <vector>
#ifndef POOL_NAMESPACE
#define POOL_NAMESPACE study_handles
#endif
namespace pool=POOL_NAMESPACE;
static bool fail_next=false;
void* operator new(std::size_t n){if(fail_next){fail_next=false;throw std::bad_alloc();}if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p) noexcept {std::free(p);}
void operator delete(void* p,std::size_t) noexcept {std::free(p);}
#define CHECK(e) do{if(!(e)){std::fprintf(stderr,"CHECK failed %d: %s\n",__LINE__,#e);std::exit(1);}}while(false)
static int alive=0;
struct Item {int value;explicit Item(int v):value(v){++alive;}~Item()noexcept{--alive;}};
pool::Handle other_handle();bool other_accepts(pool::Handle);
int main(){
 std::uint32_t zero=0;CHECK(pool::take_stamp(zero)==0&&zero==0);
 std::uint32_t edge=(std::numeric_limits<std::uint32_t>::max)()-1;
 CHECK(pool::take_stamp(edge)==(std::numeric_limits<std::uint32_t>::max)()-1);
 CHECK(pool::take_stamp(edge)==(std::numeric_limits<std::uint32_t>::max)()&&edge==0);
 CHECK(pool::take_stamp(edge)==0&&edge==0);
 {pool::HandlePool<Item> a,b;
  CHECK(!a.find(0)&&!a.erase(0)&&!a.insert(nullptr));
  auto candidate=std::make_unique<Item>(4);auto stamp=pool::next_stamp;
  fail_next=true;const auto failed=a.insert(std::move(candidate));
  CHECK(failed==0&&!fail_next&&alive==0&&pool::next_stamp==stamp);
  auto first=a.insert(std::make_unique<Item>(11));CHECK(first&&a.size()==1&&a.find(first)->value==11);
  auto* borrowed=a.find(first);std::vector<pool::Handle> handles;
  for(int i=0;i<512;++i){auto h=a.insert(std::make_unique<Item>(i));CHECK(h);handles.push_back(h);}
  CHECK(a.find(first)==borrowed&&borrowed->value==11&&a.size()==513);
  const auto& ca=a;CHECK(ca.find(first)==borrowed);
  auto foreign=b.insert(std::make_unique<Item>(22));CHECK(foreign!=first&&!a.find(foreign)&&!b.find(first));
  auto across_tu=other_handle();CHECK(across_tu!=first&&!a.find(across_tu)&&!other_accepts(first)&&other_accepts(across_tu));
  auto copy=first;CHECK(a.erase(first)&&!a.find(copy)&&!a.erase(copy));
  auto replacement=a.insert(std::make_unique<Item>(33));CHECK(replacement&&replacement!=copy);
  CHECK(std::uint32_t(replacement)==std::uint32_t(copy)&&!a.find(copy)&&a.find(replacement)->value==33);
  for(auto bad:{pool::Handle{1},pool::Handle{1}<<32,(std::numeric_limits<pool::Handle>::max)(),pool::make_handle(1,0)})CHECK(!a.find(bad)&&!a.erase(bad));
  for(auto h:handles)CHECK(a.find(h));
  a.clear();CHECK(!a.find(replacement)&&a.size()==0&&alive==1);a.clear();
  auto after_clear=a.insert(std::make_unique<Item>(44));CHECK(after_clear&&after_clear!=replacement&&!a.find(replacement));
  int sum=0;a.for_each([&](Item& v){sum+=v.value;});CHECK(sum==44);
  CHECK(b.find(foreign)->value==22);
 }
 CHECK(alive==0);
 // Exercise terminal exhaustion in this isolated test process; never reset in an application.
 pool::next_stamp=(std::numeric_limits<std::uint32_t>::max)();
 {pool::HandlePool<Item> p;auto last=p.insert(std::make_unique<Item>(55));CHECK(last&&pool::next_stamp==0);
  const auto refused=p.insert(std::make_unique<Item>(66));
  CHECK(!refused&&p.size()==1&&alive==1&&p.find(last)->value==55);
  CHECK(p.erase(last));p.clear();CHECK(!p.insert(std::make_unique<Item>(77)));}
 CHECK(alive==0);std::puts("Pool: stale/foreign/clear/cross-TU handles, growth, allocation cleanup, invalid halves and nonwrapping exhaustion passed");
}
