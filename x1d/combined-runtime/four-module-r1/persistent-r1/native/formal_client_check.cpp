/* 仅本机线程/套接字功能检查；不实例化无线对象，不打开设备。 */
#include "formal_direct_check.h"
#include <cstring>
int main(int argc,char **argv) {
    if(argc==2 && !std::strcmp(argv[1],"--check-direct")) return formal_direct_check();
    return 64;
}
