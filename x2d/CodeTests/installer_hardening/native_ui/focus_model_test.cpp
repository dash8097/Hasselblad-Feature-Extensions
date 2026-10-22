#include "focus_model.hpp"
#include <cstdio>
#include <cstdlib>
#include <limits>
using namespace x2dui;
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"failure line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
static FocusSnapshot base(){FocusSnapshot s;s.valid=s.selectable=true;for(auto&x:s.allowed)x=true;s.mode=1;return s;}
int main(){
 for(int start=1;start<=3;start++)for(int target=1;target<=3;target++){
  FocusModel m;auto s=base();s.mode=start;m.observe(s,100);int out=99;auto action=m.choose(target-1,200,out);CHECK(out==target);
  if(start==target){CHECK(action==FocusAction::Close);CHECK(m.closed());CHECK(m.choose(target-1,201,out)==FocusAction::None);continue;}
  CHECK(action==FocusAction::SetMode);CHECK(!m.closed()&&m.pending()==target);CHECK(m.choose(target-1,201,out)==FocusAction::None);s.mode=start;m.observe(s,3199);CHECK(!m.closed()&&m.pending()==target);s.mode=target;m.observe(s,3200);CHECK(m.closed()&&!m.failed());
 }
 {FocusModel m;auto s=base();m.observe(s,0);int out=0;CHECK(m.choose(1,5,out)==FocusAction::SetMode);m.observe(s,3005);CHECK(m.failed()&&!m.closed()&&m.pending()<0);s.mode=2;m.observe(s,3006);CHECK(m.failed()&&!m.closed());CHECK(m.choose(1,3007,out)==FocusAction::Close);}
 {FocusModel m;auto s=base();m.observe(s,0);int out=0;m.choose(1,0,out);s.mode=2;m.observe(s,3001);CHECK(m.failed()&&!m.closed());}
 {FocusModel m;auto s=base();m.observe(s,0);int out=0;m.choose(1,0,out);m.reject_pending();CHECK(m.failed()&&!m.closed());CHECK(m.choose(2,1,out)==FocusAction::SetMode);m.abort();CHECK(m.closed()&&m.pending()<0);s.mode=3;m.observe(s,2);CHECK(m.closed());}
 for(int mask=0;mask<8;mask++){FocusModel m;auto s=base();for(int i=0;i<3;i++)s.allowed[i]=(mask&(1<<i))!=0;m.observe(s,0);for(int i=0;i<3;i++)CHECK(m.allowed(i)==s.allowed[i]);m.move(1);int expected=s.allowed[1]?1:s.allowed[2]?2:0;CHECK(m.selected()==expected);m.move(-1);CHECK(m.selected()==(expected>0&&s.allowed[0]?0:expected));}
 for(int mode=0;mode<2;mode++){FocusModel m;auto s=base();if(mode)s.valid=false;else s.selectable=false;m.observe(s,0);int out=123;for(int i=-2;i<5;i++)CHECK(m.choose(i,0,out)==FocusAction::None);CHECK(out==123);}
 {FocusModel m;auto s=base();m.observe(s,0);int out=123;CHECK(m.choose(1,std::numeric_limits<uint64_t>::max()-2999,out)==FocusAction::None);CHECK(out==123&&m.pending()<0);CHECK(m.choose(1,std::numeric_limits<uint64_t>::max()-3000,out)==FocusAction::SetMode);s.mode=2;m.observe(s,std::numeric_limits<uint64_t>::max());CHECK(m.closed());}
 {FocusModel m;auto s=base();s.mode=255;m.observe(s,0);CHECK(m.selected()==-1);m.move(1);CHECK(m.selected()==0);s.mode=3;m.observe(s,1);CHECK(m.selected()==2);}
 for(int language=-1;language<13;language++)for(int field=0;field<5;field++)CHECK(focus_text(language,field)[0]!=0);
 CHECK(focus_text(0,-1)[0]==0&&focus_text(0,5)[0]==0);
 std::printf("FOCUS_MODEL_PASSED assertions=%u camera_requests=0\n",checks);
}
