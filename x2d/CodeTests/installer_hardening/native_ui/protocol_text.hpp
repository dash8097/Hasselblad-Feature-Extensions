#pragma once
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
namespace x2dui {
// 本模块私有、容量受限文本。无异常/全局new覆盖/第二套C++运行库。
// 分配或容量失败会粘住 valid=false；绝不截断成一个貌似有效的协议字段。
template<class C> class BoundedText {
 C* data_=nullptr;size_t size_=0,capacity_=0;bool valid_=true;
 static size_t length(const C*p){size_t n=0;if(p)while(p[n])++n;return n;}
 bool reserve(size_t n){if(!valid_||n>4096){valid_=false;return false;}if(n<=capacity_)return true;
  auto capacity=(n+31)&~size_t(31);auto*p=static_cast<C*>(std::realloc(data_,(capacity+1)*sizeof(C)));if(!p){valid_=false;return false;}data_=p;capacity_=capacity;return true;
 }
public:
 BoundedText()noexcept=default;
 BoundedText(const C*p){append(p,length(p));}
 BoundedText(const C*p,size_t n){append(p,n);}
 template<class S,class=decltype(((const S*)nullptr)->data())> BoundedText(const S&s):BoundedText(s.data(),s.size()){}
 BoundedText(const BoundedText&o){valid_=o.valid_;if(valid_)append(o.c_str(),o.size_);}
 BoundedText(BoundedText&&o)noexcept:data_(o.data_),size_(o.size_),capacity_(o.capacity_),valid_(o.valid_){o.data_=nullptr;o.size_=o.capacity_=0;o.valid_=true;}
 ~BoundedText(){std::free(data_);}
 BoundedText&operator=(const BoundedText&o){if(this!=&o){BoundedText n(o);swap(n);}return *this;}
 BoundedText&operator=(BoundedText&&o)noexcept{swap(o);return *this;}
 BoundedText&operator=(const C*p){BoundedText n(p);swap(n);return *this;}
 void swap(BoundedText&o)noexcept{auto*p=data_;data_=o.data_;o.data_=p;auto n=size_;size_=o.size_;o.size_=n;n=capacity_;capacity_=o.capacity_;o.capacity_=n;bool v=valid_;valid_=o.valid_;o.valid_=v;}
 bool valid()const noexcept{return valid_;}void invalidate()noexcept{valid_=false;}
 const C*c_str()const noexcept{static const C zero=0;return data_?data_:&zero;}
 const C*data()const noexcept{return c_str();}
 size_t size()const noexcept{return size_;}bool empty()const noexcept{return size_==0;}
 C operator[](size_t n)const noexcept{return n<size_?data_[n]:C{};}
 C back()const noexcept{return size_?data_[size_-1]:C{};}
 const C*begin()const noexcept{return c_str();}const C*end()const noexcept{return c_str()+size_;}
 void clear()noexcept{size_=0;valid_=true;if(data_)data_[0]=0;}
 void pop_back()noexcept{if(size_)data_[--size_]=0;}
 BoundedText substr(size_t start,size_t count=4096)const{if(!valid_||start>size_){BoundedText r;r.invalidate();return r;}if(count>size_-start)count=size_-start;return BoundedText(c_str()+start,count);}
 void append(const C*p,size_t n){if(n>4096||size_>4096-n||(!p&&n)){invalidate();return;}if(!n)return;if(!reserve(size_+n))return;std::memcpy(data_+size_,p,n*sizeof(C));size_+=n;data_[size_]=0;}
 void append(const BoundedText&s){if(!s.valid_){invalidate();return;}if(this==&s){BoundedText copy(s);append(copy);return;}append(s.c_str(),s.size_);}
 BoundedText&operator+=(const BoundedText&s){append(s);return *this;}
 BoundedText&operator+=(const C*p){append(p,length(p));return *this;}
 BoundedText&operator+=(C c){append(&c,1);return *this;}
 friend bool operator==(const BoundedText&a,const BoundedText&b){return a.valid_&&b.valid_&&a.size_==b.size_&&(!a.size_||!std::memcmp(a.c_str(),b.c_str(),a.size_*sizeof(C)));}
 friend bool operator!=(const BoundedText&a,const BoundedText&b){return !(a==b);}
 friend bool operator==(const BoundedText&a,const C*b){auto n=length(b);return a.valid_&&a.size_==n&&(!n||!std::memcmp(a.c_str(),b,n*sizeof(C)));}
 friend bool operator==(const C*a,const BoundedText&b){return b==a;}
 friend bool operator!=(const BoundedText&a,const C*b){return !(a==b);}
 friend bool operator!=(const C*a,const BoundedText&b){return !(b==a);}
 friend BoundedText operator+(const BoundedText&a,const BoundedText&b){BoundedText r(a);r+=b;return r;}
};
using ProtocolText=BoundedText<char>;
using UiText=BoundedText<char16_t>;
struct ProtocolBytes {
 const char*p="";size_t n=0;
 ProtocolBytes()=default;ProtocolBytes(const char*s):p(s?s:""),n(s?std::strlen(s):0){}
 ProtocolBytes(const char*s,size_t size):p(s?s:""),n(s?size:0){}
 template<class S,class=decltype(((const S*)nullptr)->data())> ProtocolBytes(const S&s):p(s.data()),n(s.size()){}
 size_t size()const noexcept{return n;}const char*data()const noexcept{return p;}
 char operator[](size_t i)const noexcept{return i<n?p[i]:0;}
 ProtocolBytes substr(size_t start,size_t count=~size_t(0))const noexcept{if(start>n)return {};if(count>n-start)count=n-start;return {p+start,count};}
 friend bool operator==(ProtocolBytes a,ProtocolBytes b)noexcept{return a.n==b.n&&(!a.n||!std::memcmp(a.p,b.p,a.n));}
 friend bool operator!=(ProtocolBytes a,ProtocolBytes b)noexcept{return !(a==b);}
};
inline ProtocolText protocol_number(uint64_t v){char buf[21];size_t at=sizeof(buf);do{buf[--at]=char('0'+v%10);v/=10;}while(v);return {buf+at,sizeof(buf)-at};}
}
