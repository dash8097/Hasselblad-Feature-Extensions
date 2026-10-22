/* 私有叶子适配：运行冻结完整memory状态机；不模拟其restore/cleanup。 */
#include "native_common.hpp"
#include "native_memory_runner.hpp"
#include <signal.h>
#include <unistd.h>
static volatile sig_atomic_t interrupted,recovering;
static void sig(int){interrupted=1;}
extern "C" int cn_cancel_requested(){return interrupted&&!recovering;}
extern "C" int cn_signal_pending(){return interrupted;}
extern "C" void cn_recovery_mode(int on){if(on)recovering++;else if(recovering)recovering--;}
static std::string scenario(){return cn::value("/tmp/scenario");}
extern "C" int cn_prepare_main(int,char**){return 0;}
extern "C" int cn_counter_main(int,char**){return 0;}
extern "C" int cn_heif_restart_main(int,char**){return 0;}
extern "C" int cn_publish_main(int,char**argv){auto s=std::string(argv[2]);cn::write_new(s+"/test.publish.called","1\n");if(scenario()=="publish-failure")return 17;cn::write_new(s+"/album-location","100HASBL/A0000001.JPG\n");return 0;}
extern "C" int cn_render_main(int,char**argv){
    if(cn::get("camera","live_view_state","int32")!="0"||cn::get("camera","stream_group","int32")!="255")return 31;
    auto s=std::string(argv[2]);cn::write_new(s+"/render.started","1\n");cn::write_new(s+"/test.render.called","1\n");
    if(scenario()!="missing-source-drain")cn::write_new(s+"/memory.consumers.drained","MEMORY_CONSUMERS_DRAINED_V1\n");
    cn::write_new(s+"/render.drained","RENDER_DRAINED_V1\n");cn::write_new(s+"/render.cleaned","RENDER_CLEANED_V1\n");
    if(scenario()=="render-failure")return 17;
    cn::write_new(s+"/render.restored","1\n");cn::write_new(s+"/memory.ready","1\n");return 0;
}
int main(int argc,char**argv){signal(SIGTERM,sig);signal(SIGINT,sig);return cn_memory_runner_main(argc,argv);}
