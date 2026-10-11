#include "../joint/joint_policy.h"
extern "C" int joint_policy_case() { return jointWindowActive(); }
extern "C" int joint_gpu_case(int maximum,int bgra,int npot) { return gpuAllowed(maximum,bgra!=0,npot!=0); }
