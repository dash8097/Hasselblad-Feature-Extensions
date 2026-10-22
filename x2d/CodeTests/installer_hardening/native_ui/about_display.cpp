#include "about_display.hpp"
namespace x2dui {
UiText about_display_text(const char*name,bool about,bool right,const UiText&old,const UiText&formatted){
 if(!name||!about||!right||!old.valid()||!formatted.valid())return old;
 if(std::strcmp(name,"product_name")&&std::strcmp(name,"model_number"))return old;
 bool found=false;for(size_t i=0;i+4<=old.size();i++)if(old[i]=='1'&&old[i+1]=='0'&&old[i+2]=='0'&&old[i+3]=='C'){found=true;break;}
 if(!found)return old;UiText result;
 for(size_t i=0;i<formatted.size();i++){
  if(i+4<=formatted.size()&&formatted[i]=='1'&&formatted[i+1]=='0'&&formatted[i+2]=='0'&&formatted[i+3]=='C'){result+=u"1600C MS";i+=3;}else result+=formatted[i];
 }
 return result.valid()?result:old;
}
}
