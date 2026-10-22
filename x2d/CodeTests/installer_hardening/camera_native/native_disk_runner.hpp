#pragma once
/* X2D 4.2.0 H3 过渡候选。未由独立 checker 放行，不操作硬件。
 * sixteen-run JOB DELAY 16 --prepare
 * sixteen-restore JOB
 * sixteen-watchdog JOB OWNER_PID
 * quarter-run JOB METHOD SECONDS NOISE DELAY --prepare
 * quarter-restore JOB
 * quarter-watchdog JOB OWNER_PID
 * 不接受任意程序或 shell 文本。 */
extern "C" int cn_disk_runner_main(int argc,char **argv);
