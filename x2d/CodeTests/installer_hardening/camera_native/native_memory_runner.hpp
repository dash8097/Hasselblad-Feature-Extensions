#pragma once
/* 内部候选；X2D 4.2.0 / H3 p118 SHA3908f163...920beb。
 * 尚未接入 worker/build，必须由独立 checker 审核后再考虑集成。
 * memory-run JOB DELAY 6 --prepare
 * memory-restore JOB
 * memory-watchdog JOB OWNER_PID
 * 不接受任意命令、环境或脚本输入。
 */
extern "C" int cn_memory_runner_main(int argc,char **argv);
