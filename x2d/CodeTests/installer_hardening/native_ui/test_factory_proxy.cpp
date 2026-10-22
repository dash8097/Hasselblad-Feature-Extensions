#include "factory_proxy.hpp"
#include "esflash_timing.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
using namespace x2dui;
static unsigned reads,writes,checks;struct Fake{uint32_t values[180]{};};
static void check(bool b,const char*n){++checks;if(!b){fprintf(stderr,"FACTORY_PROXY_FAIL %s\n",n);exit(1);}}
static void meta(void*object,int op,int index,void**args){auto*s=static_cast<Fake*>(object);unsigned width=index==38||index==46||index==54||index==88||index==100||index==11||index==99||index==51?1:4;if(op==1){++reads;std::memcpy(args[0],&s->values[index],width);}else if(op==2){++writes;s->values[index]=0;std::memcpy(&s->values[index],args[0],width);}}
int main(){Fake camera,system,storage;void*objects[]={&camera,&system,&storage};FactoryProxyProfile p{};for(unsigned n=0;n<3;n++){p.slots[n]=&objects[n];p.call[n]=meta;p.meta[n]=&objects[n];}p.cast=[](const void*m,const void*o)->const void*{return *static_cast<void*const*>(m)==o?o:nullptr;};
 FactoryFieldSpec specs[]={{0,100,1,false,false,false},{0,46,1,false,false,true},{0,54,1,false,false,true},{0,88,1,false,false,true},{0,38,1,true,false,true},{1,30,4,false,true,true},{1,32,4,false,false,false},{1,33,4,false,false,true},{2,11,1,true,false,false},{2,0,4,false,false,false}};for(unsigned n=0;n<10;n++)p.fields[n]=specs[n];
 int64_t value=999;camera.values[100]=255;check(factory_read(p,FactoryField::ImageFormat,value)&&value==255,"unsigned-byte-zero-extended");check(!factory_write(p,FactoryField::ImageFormat,0)&&!writes,"read-only-policy");check(factory_write(p,FactoryField::DriveMode,255)&&camera.values[46]==255,"byte-write");auto count=writes;check(!factory_write(p,FactoryField::DriveMode,256)&&writes==count,"byte-overflow-rejected");check(!factory_write(p,FactoryField::DriveMode,-1),"negative-enum-rejected");
 check(factory_write(p,FactoryField::ScreenTimeout,-1)&&factory_read(p,FactoryField::ScreenTimeout,value)&&value==-1,"signed-timeout-minus-one");check(!factory_write(p,FactoryField::ScreenTimeout,INT64_C(2147483648)),"signed-overflow-rejected");check(factory_write(p,FactoryField::ScreensAllowed,UINT32_MAX)&&factory_read(p,FactoryField::ScreensAllowed,value)&&value==UINT32_MAX,"unsigned32-retained");
 check(factory_write(p,FactoryField::EvfOnly,1)&&factory_read(p,FactoryField::EvfOnly,value)&&value==1,"bool-write-read");check(!factory_write(p,FactoryField::EvfOnly,2),"bool-range");objects[0]=nullptr;count=reads;check(!factory_read(p,FactoryField::DriveMode,value)&&reads==count,"missing-proxy-not-created");check(!factory_write(p,FactoryField::DriveMode,0),"missing-write-rejected");check(!factory_read(p,FactoryField::Count,value),"unknown-field");
 objects[0]=&camera;
 auto field=[&](FactoryField f,unsigned index,unsigned width,bool boolean,bool sign){p.fields[unsigned(f)]={0,index,width,boolean,sign,false};};
 field(FactoryField::ImageBitDepth,99,1,false,false);field(FactoryField::ShutterCurrent,171,4,false,true);field(FactoryField::ShutterMin,174,4,false,true);field(FactoryField::ShutterMax,173,4,false,true);field(FactoryField::ElectronicShutter,51,1,true,false);field(FactoryField::ExposureStatus,65,4,false,false);
 camera.values[174]=uint32_t(-144);camera.values[173]=150;camera.values[171]=32;camera.values[99]=0;camera.values[51]=1;camera.values[65]=0;
 auto e=flash_exposure(p);check(e.valid&&e.idle&&e.profile()==2,"snapshot-14-hss-signed-tv-bounds");
 camera.values[99]=1;camera.values[171]=16;e=flash_exposure(p);check(e.valid&&e.profile()==3,"snapshot-16-normal");
 camera.values[171]=20;e=flash_exposure(p);check(e.profile()==4,"snapshot-16-boundary");
 camera.values[99]=255;check(!flash_exposure(p).valid,"unknown-depth-rejected");camera.values[99]=1;
 camera.values[171]=151;check(!flash_exposure(p).valid,"out-of-range-tv-rejected");camera.values[171]=20;
 camera.values[65]=1;e=flash_exposure(p);check(e.valid&&!e.idle,"active-exposure-not-idle");camera.values[65]=0;
 camera.values[51]=0;e=flash_exposure(p);check(e.valid&&!e.electronic&&!e.profile(),"mechanical-not-armed");
 count=writes;for(unsigned n=unsigned(FactoryField::ImageBitDepth);n<unsigned(FactoryField::Count);n++)check(!factory_write(p,FactoryField(n),0),"new-fields-read-only");check(writes==count,"no-camera-setting-writes");
 objects[0]=nullptr;check(!flash_exposure(p).valid,"missing-camera-proxy-no-guess");
 printf("NATIVE_FACTORY_PROXY_PASS assertions=%u proxy_objects=explicit_width_fixtures camera_requests=0\n",checks);
}
