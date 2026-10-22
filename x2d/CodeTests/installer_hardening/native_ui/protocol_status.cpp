#include "protocol_status.hpp"
#include <cfloat>
#include <cstdlib>
#include <initializer_list>

namespace x2dui {
const char* pixel_endpoint_path(PixelEndpoint e) noexcept {
 switch(e){case PixelEndpoint::Status:return "/tmp/x2d-pixel-overclock/status.json";
 case PixelEndpoint::Shutter:return "/tmp/x2d-pixel-overclock/shutter-status.json";
 case PixelEndpoint::QuarterStatus:return "/tmp/x2d-quarter-native.status";
 case PixelEndpoint::Arm:return "/tmp/x2d-pixel-overclock/arm.txt";
 case PixelEndpoint::Quarter:return "/tmp/x2d-pixel-overclock/quarter.txt";
 case PixelEndpoint::Finish:return "/tmp/x2d-pixel-overclock/finish.txt";}return nullptr;
}
const ProtocolValue* ProtocolObject::get(const char*k)const noexcept{for(size_t i=0;i<count;++i)if(values[i].key==k)return &values[i].value;return nullptr;}
bool ProtocolObject::is_boolean(const char*k)const noexcept{auto p=get(k);return p&&p->type==ProtocolValue::Type::Boolean;}
bool ProtocolObject::boolean(const char*k)const noexcept{auto p=get(k);return is_boolean(k)&&p->boolean;}
bool ProtocolObject::integer(const char*k,uint64_t lo,uint64_t hi,uint64_t&out)const noexcept{
 auto p=get(k);if(!p||p->type!=ProtocolValue::Type::Number||p->number<double(lo)||p->number>double(hi)||p->number>4294967295.0||double(uint64_t(p->number))!=p->number)return false;
 out=uint64_t(p->number);return true;
}
uint64_t ProtocolObject::integer_or(const char*k,uint64_t lo,uint64_t hi,uint64_t fallback)const noexcept{uint64_t v;return integer(k,lo,hi,v)?v:fallback;}
ProtocolText ProtocolObject::string(const char*k)const{auto p=get(k);return p&&p->type==ProtocolValue::Type::String?p->string:ProtocolText{};}
bool protocol_session(const ProtocolText&s)noexcept{if(!s.valid()||s.size()!=8)return false;for(auto c:s)if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return false;return true;}
namespace {
bool utf8(ProtocolBytes s,size_t&i,uint32_t&value){
 if(i==s.size())return false;unsigned c=uint8_t(s[i++]);if(c<128){value=c;return true;}
 unsigned n=0;uint32_t v=0,min=0;
 if(c>=0xc2&&c<=0xdf){n=1;v=c&31;min=128;}else if(c>=0xe0&&c<=0xef){n=2;v=c&15;min=2048;}else if(c>=0xf0&&c<=0xf4){n=3;v=c&7;min=65536;}else return false;
 for(unsigned j=0;j<n;++j){if(i==s.size())return false;unsigned x=uint8_t(s[i++]);if((x&192)!=128)return false;v=(v<<6)|(x&63);}
 if(v<min||v>0x10ffff||(v>=0xd800&&v<=0xdfff))return false;value=v;return true;
}
void encode(ProtocolText&s,uint32_t c){if(c<128)s+=char(c);else if(c<2048){s+=char(0xc0|(c>>6));s+=char(0x80|(c&63));}else if(c<65536){s+=char(0xe0|(c>>12));s+=char(0x80|((c>>6)&63));s+=char(0x80|(c&63));}else{s+=char(0xf0|(c>>18));s+=char(0x80|((c>>12)&63));s+=char(0x80|((c>>6)&63));s+=char(0x80|(c&63));}}
struct Parser {
 ProtocolBytes s;size_t at=0,nodes=0;bool good=true;
 void space(){while(at<s.size()&&(s[at]==' '||s[at]=='\r'||s[at]=='\n'||s[at]=='\t'))++at;}
 bool eat(char c){space();if(at<s.size()&&s[at]==c){++at;return true;}return false;}
 unsigned hex(){unsigned v=0;for(int n=0;n<4;++n){if(at==s.size()){good=false;return 0;}char c=s[at++];v<<=4;if(c>='0'&&c<='9')v+=c-'0';else if(c>='a'&&c<='f')v+=c-'a'+10;else if(c>='A'&&c<='F')v+=c-'A'+10;else{good=false;return 0;}}return v;}
 ProtocolText string(){ProtocolText r;if(!eat('"')){good=false;return r;}
  while(good&&at<s.size()){auto c=uint8_t(s[at++]);if(c=='"'){good=r.valid();return r;}if(c<32){good=false;break;}
   if(c=='\\'){if(at==s.size()){good=false;break;}c=uint8_t(s[at++]);switch(c){case '"':case '\\':case '/':r+=char(c);break;case 'b':r+='\b';break;case 'f':r+='\f';break;case 'n':r+='\n';break;case 'r':r+='\r';break;case 't':r+='\t';break;
    case 'u':{uint32_t u=hex();if(u>=0xd800&&u<=0xdbff){if(at+2>s.size()||s[at++]!='\\'||s[at++]!='u'){good=false;break;}auto l=hex();if(l<0xdc00||l>0xdfff){good=false;break;}u=0x10000+((u-0xd800)<<10)+(l-0xdc00);}else if(u>=0xdc00&&u<=0xdfff){good=false;break;}encode(r,u);break;}default:good=false;break;}
   }else if(c>=128){--at;auto start=at;uint32_t code=0;if(!utf8(s,at,code)){good=false;break;}r.append(s.data()+start,at-start);}else r+=char(c);
   if(!r.valid()){good=false;break;}
  }good=false;return r;
 }
 bool object(ProtocolObject&o,unsigned depth){if(!eat('{'))return good=false;if(eat('}'))return true;
  do{auto k=string();if(!good||!eat(':')||o.count==64)return good=false;for(size_t i=0;i<o.count;++i)if(k==o.values[i].key)return good=false;
   auto&entry=o.values[o.count++];entry.key=static_cast<ProtocolText&&>(k);entry.value=value(depth+1);if(!good)return false;
  }while(eat(','));return good=eat('}');
 }
 ProtocolValue value(unsigned depth){ProtocolValue v;if(++nodes>256||depth>4){good=false;return v;}space();if(at==s.size()){good=false;return v;}
  if(s[at]=='"'){v.type=ProtocolValue::Type::String;v.string=string();return v;}
  if(s[at]=='{'){v.type=ProtocolValue::Type::Container;ProtocolObject ignored;object(ignored,depth);return v;}
  if(eat('[')){v.type=ProtocolValue::Type::Container;if(eat(']'))return v;do{value(depth+1);if(!good)return v;}while(eat(','));good=eat(']');return v;}
  for(auto lit:{"true","false","null"}){ProtocolBytes l(lit);if(s.substr(at,l.size())==l){at+=l.size();v.type=l=="null"?ProtocolValue::Type::Null:ProtocolValue::Type::Boolean;v.boolean=l=="true";return v;}}
  size_t start=at;bool negative=s[at]=='-';if(negative)++at;if(at==s.size()){good=false;return v;}
  double number=0;int fraction=0,exp=0;bool expNegative=false;
  if(s[at]=='0')++at;else{if(s[at]<'1'||s[at]>'9'){good=false;return v;}while(at<s.size()&&s[at]>='0'&&s[at]<='9'){number=number*10+(s[at++]-'0');if(number>DBL_MAX){good=false;return v;}}}
  if(at<s.size()&&s[at]=='.'){++at;auto before=at;while(at<s.size()&&s[at]>='0'&&s[at]<='9'){number=number*10+(s[at++]-'0');++fraction;if(number>DBL_MAX){good=false;return v;}}if(at==before){good=false;return v;}}
  if(at<s.size()&&(s[at]=='e'||s[at]=='E')){++at;if(at<s.size()&&(s[at]=='+'||s[at]=='-'))expNegative=s[at++]=='-';auto before=at;while(at<s.size()&&s[at]>='0'&&s[at]<='9'){exp=exp<10000?exp*10+(s[at]-'0'):10000;++at;}if(at==before){good=false;return v;}}
  if(at-start>512){good=false;return v;}exp=(expNegative?-exp:exp)-fraction;
  if(exp>400&&number!=0){good=false;return v;}if(exp<-400)number=0;else while(exp<0){number/=10;++exp;}
  while(exp>0&&number!=0){number*=10;--exp;if(number>DBL_MAX){good=false;return v;}}
  v.number=negative?-number:number;v.type=ProtocolValue::Type::Number;return v;
 }
};
}
bool parse_protocol(ProtocolBytes input,ProtocolObject&out)noexcept{if(input.size()>16384)return false;Parser p{input};ProtocolObject result;if(!p.object(result,0))return false;p.space();if(!p.good||p.at!=input.size())return false;out=static_cast<ProtocolObject&&>(result);return true;}
UiText protocol_utf16(ProtocolBytes s){UiText out;size_t i=0;while(i<s.size()){uint32_t c=0;if(!utf8(s,i,c)){out.invalidate();return out;}if(c<=65535)out+=char16_t(c);else{c-=65536;out+=char16_t(0xd800+(c>>10));out+=char16_t(0xdc00+(c&1023));}if(!out.valid())return out;}return out;}
}
