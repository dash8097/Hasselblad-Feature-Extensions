#include "native_ui_contract.hpp"
#include "native_ui_ready.hpp"
#include "native_common.hpp"
#include <cstdio>
extern "C" int cn_ui_check_main(int argc,char**){
 if(argc!=2)return 2;
#if CN_NATIVE_UI_ENABLED
 try{cn::native_ui_ready({CN_NATIVE_UI_SHA256,CN_NATIVE_GUI_SHA256,CN_NATIVE_UI_CAPABILITIES},2000);puts("NATIVE_UI_READY_CURRENT_REQUEST");return 0;}
 catch(const cn::Error&e){fprintf(stderr,"NATIVE_UI_CHECK_FAILED step=%s rc=%d\n",e.what(),e.rc);return e.rc;}
 catch(const std::exception&e){fprintf(stderr,"NATIVE_UI_CHECK_FAILED detail=%s\n",e.what());return 1;}
#else
 fputs("NATIVE_UI_CHECK_UNAVAILABLE_LEGACY_BUILD\n",stderr);return 2;
#endif
}
