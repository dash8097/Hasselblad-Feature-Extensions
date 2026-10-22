#pragma once
/* 内部原生迁移候选；必须绑定新增 capture abort/drain 合同并独立验证。
 * stack-run JOB METHOD SECONDS_OR_ISO NOISE INITIAL_DELAY --prepare
 * stack-restore JOB / stack-watchdog JOB OWNER_PID
 */
extern "C" int cn_stack_runner_main(int argc,char **argv);
