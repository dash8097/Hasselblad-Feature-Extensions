#include "pixel_viewport_core.hpp"
namespace x2dui {
bool PixelViewportCore::valid_source(ProtocolBytes s,unsigned w,unsigned h)noexcept{
 if(w!=23326||h!=17498)return false;
 constexpr char prefix[]="image://imagestorefull/ssd/";constexpr unsigned n=sizeof(prefix)-1;
 if(s.size()!=n+17&&s.size()!=n+21)return false;
 for(unsigned i=0;i<n;i++)if(s[i]!=prefix[i])return false;
 if(s[n]<'1'||s[n]>'9'||s[n+1]<'0'||s[n+1]>'9'||s[n+2]<'0'||s[n+2]>'9')return false;
 constexpr char mid[]="HASBL/";for(unsigned i=0;i<6;i++)if(s[n+3+i]!=mid[i])return false;
 if(s[n+9]<'A'||s[n+9]>'Z')return false;for(unsigned i=10;i<17;i++)if(s[n+i]<'0'||s[n+i]>'9')return false;
 if(s.size()==n+17)return true;
 return s[n+17]=='.'&&((s[n+18]=='J'&&s[n+19]=='P'&&s[n+20]=='G')||(s[n+18]=='3'&&s[n+19]=='F'&&s[n+20]=='R'));
}
bool PixelViewportCore::set(ProtocolBytes s,unsigned w,unsigned h,unsigned t,uint64_t now){
 if(source_==ProtocolText(s.data(),s.size())&&width_==w&&height_==h&&texture_==t)return false;
 ProtocolText next(s.data(),s.size());source_=static_cast<ProtocolText&&>(next);decode_.clear();width_=w;height_=h;texture_=t;count_=readyTiles=failedTiles=0;generation_++;armed_=false;
 for(auto&v:tileState_)v=0;
 if(!source_.valid()||!valid_source(s,w,h)||!t||t>32768||now>UINT64_MAX-50)return true;
 unsigned columns=(w+t-1)/t,rows=(h+t-1)/t;if(columns>256||rows>256||columns*rows>256)return true;
 armed_=true;due_=now+50;return true;
}
bool PixelViewportCore::tick(uint64_t now){if(!armed_||now<due_)return false;armed_=false;decode_=source_;if(!decode_.valid())return false;count_=((width_+texture_-1)/texture_)*((height_+texture_-1)/texture_);requested_=now;regionRequests++;return true;}
bool PixelViewportCore::adopt(ProtocolBytes s,unsigned w,unsigned h,unsigned t,uint64_t now){if(!set(s,w,h,t,now))return false;if(!armed_)return true;due_=now;tick(now);return true;}
bool PixelViewportCore::tile(unsigned i,ViewportTile&o)const noexcept{if(i>=count_||!texture_)return false;unsigned columns=(width_+texture_-1)/texture_;o.x=(i%columns)*texture_;o.y=(i/columns)*texture_;o.width=width_-o.x<texture_?width_-o.x:texture_;o.height=height_-o.y<texture_?height_-o.y:texture_;return true;}
ProtocolText PixelViewportCore::tile_source(unsigned i)const{ViewportTile r;if(!tile(i,r))return {};return decode_+"@"+protocol_number(r.width)+"x"+protocol_number(r.height)+":"+protocol_number(r.x)+","+protocol_number(r.y);}
ViewportStatus PixelViewportCore::status()const noexcept{return failedTiles?ViewportStatus::Error:count_&&readyTiles==count_?ViewportStatus::Ready:count_?ViewportStatus::Loading:ViewportStatus::Null;}
void PixelViewportCore::observe(uint64_t gen,unsigned i,unsigned image,bool lutUsed,unsigned lut,uint64_t now){if(gen!=generation_||i>=count_)return;auto before=status();bool ready=image==1&&(!lutUsed||lut==1),failed=image==3||lut==3;unsigned char state=(ready?1:0)|(failed?2:0);auto old=tileState_[i];if((old&1)!=(state&1))readyTiles=ready?readyTiles+1:readyTiles-1;if((old&2)!=(state&2))failedTiles=failed?failedTiles+1:failedTiles-1;tileState_[i]=state;if(before!=ViewportStatus::Ready&&status()==ViewportStatus::Ready)regionReadyMs=now>=requested_?now-requested_:0;}
}
