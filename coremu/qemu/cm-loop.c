/*
 * COREMU Parallel Emulator Framework
 * The definition of core thread function
 *
 * Copyright (C) 2010 Parallel Processing Institute (PPI), Fudan Univ.
 *  <http://ppi.fudan.edu.cn/system_research_group>
 *
 * Authors:
 *  Zhaoguo Wang    <zgwang@fudan.edu.cn>
 *  Yufei Chen      <chenyufei@fudan.edu.cn>
 *  Ran Liu         <naruilone@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "cpu.h"
#include "cpus.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <sched.h>

#include "coremu-intr.h"
#include "coremu-debug.h"
#include "coremu-sched.h"
#include "coremu-types.h"
#include "coremu-core.h"
#include "cm-loop.h"
#include "cm-timer.h"
#include "cm-init.h"

/*#define ANALYTICAL_MODEL_STATISTICS
#define ANALYTICAL_MODEL_TRACE_LOG
#ifdef ANALYTICAL_MODEL_STATISTICS
#ifdef ANALYTICAL_MODEL_TRACE_LOG
#define ftrifs_trace_log(...) \
    do { \
        if (fanalytical_log_file) { \
            fprintf(fanalytical_log_file, ## __VA_ARGS__); \
            fflush(fanalytical_log_file); \
        } \
    } while (0)
#endif
#endif //*/


int cm_cpu_can_run(CPUState *);
extern int vm_running;
static bool cm_tcg_cpu_exec(void);
static bool cm_tcg_cpu_exec(void)
{
    int ret = 0;
    CPUState *env = cpu_single_env;
    struct timespec halt_interval;
    halt_interval.tv_sec = 0;
    halt_interval.tv_nsec = 10000;

    for (;;) {
        if (cm_local_alarm_pending())
            cm_run_all_local_timers();
		
	//ftrifs_trace_log("normal %d env->stop: %d vm_running: %d state: %d\n", env->cpu_index, env->stop, vm_running);
        coremu_receive_intr();
        if (cm_cpu_can_run(env))
            ret = cpu_exec(env);
        else if (env->stop)
            break;

        if (!cm_vm_can_run())
            break;

        if (ret == EXCP_DEBUG) {
            coremu_assert(0, "debug support hasn't been finished\n");
            break;
        }
        if (ret == EXCP_HALTED || ret == EXCP_HLT) {
            coremu_cpu_sched(CM_EVENT_HALTED);
        }
    }
    return ret;
}

void *cm_cpu_loop(void *args)
{
    int ret;

    /* Must initialize cpu_single_env before initializing core thread. */
    assert(args);
    cpu_single_env = (CPUState *)args;

#define ZKY_CYCLE
#ifdef ZKY_CYCLE
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET((cpu_single_env->cpu_index)%64, &mask);
    sched_setaffinity(syscall(SYS_gettid), sizeof(cpu_set_t), &mask);
    printf("thread_id is %d\n", cpu_single_env->cpu_index);
#endif

    /* Setup dynamic translator */
    cm_cpu_exec_init_core();

    for (;;) {
        ret = cm_tcg_cpu_exec();
        if (cm_test_reset_request()) {
            coremu_pause_core();
            continue;
        }
        break;
    }
    cm_stop_local_timer();
    coremu_core_exit(NULL);
    assert(0);
}
