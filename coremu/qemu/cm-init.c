/*
 * COREMU Parallel Emulator Framework
 *
 * Initialization stuff for qemu.
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

/* We include this file in exec.c */

#include <sys/types.h>
#include <sys/mman.h>
#include <pthread.h>

#define VERBOSE_COREMU
#include "sysemu.h"
#include "coremu-config.h"
#include "coremu-sched.h"
#include "coremu-debug.h"
#include "coremu-init.h"
#include "cm-features/logbuffer.h"
#include "cm-features/instrument.h"
#include "cm-features/memtrace.h"
#include "cm-timer.h"
#include "cm-init.h"

/* XXX How to clean up the following code? */

/* Since each core uses it's own code buffer, we set a large value here. */
#undef DEFAULT_CODE_GEN_BUFFER_SIZE
#define DEFAULT_CODE_GEN_BUFFER_SIZE (800 * 1024 * 1024)

static uint64_t cm_bufsize = 0;
static void *cm_bufbase = NULL;
#define min(a, b) ((a) < (b) ? (a) : (b))

/* Prepare a large code cache for each CORE to allocate later */
static void cm_code_gen_alloc_all(void)
{
    int flags = MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT;

    /*cm_bufsize = (min(DEFAULT_CODE_GEN_BUFFER_SIZE, phys_ram_size));*/
    /* XXX what if this is larger than physical ram size? */
    cm_bufsize = DEFAULT_CODE_GEN_BUFFER_SIZE;
    cm_bufbase = mmap(NULL, cm_bufsize, PROT_WRITE | PROT_READ | PROT_EXEC,
                      flags, -1, 0);

    if (cm_bufbase == MAP_FAILED) {
        coremu_assert(0, "mmap failed\n");
    }

    code_gen_buffer_size = (unsigned long)(cm_bufsize / (smp_cpus));
    coremu_assert(code_gen_buffer_size >= MIN_CODE_GEN_BUFFER_SIZE,
              "code buffer size too small");
    code_gen_buffer_max_size = code_gen_buffer_size - (TCG_MAX_OP_SIZE * OPC_MAX_SIZE);
    code_gen_max_blocks = code_gen_buffer_size / CODE_GEN_AVG_BLOCK_SIZE;
}

/* From the allocated memory in code_gen_alloc_all, we allocate memory for each
 * core. */
static void cm_code_gen_alloc(void)
{
    /* We use cpu_index here, note that this maybe not the same as architecture
     * dependent cpu id. eg. cpuid_apic_id. */
    code_gen_buffer = cm_bufbase + (code_gen_buffer_size *
                                    cpu_single_env->cpu_index);

    /* Allocate space for TBs. */
    tbs = qemu_malloc(code_gen_max_blocks * sizeof(TranslationBlock));

   /* coremu_print("CORE[%u] TC [%lu MB] at %p", cpu_single_env->cpu_index,
             (code_gen_buffer_size) / (1024 * 1024), code_gen_buffer); */
}

/* For coremu, code generator related initialization should be called by all
 * core thread. While other stuff only need to be done in the hardware
 * thread. */
void cm_cpu_exec_init(void)
{
    page_init();
    io_mem_init();
    
    /* Allocate code cache. */
    cm_code_gen_alloc_all();

    /* Code prologue initialization. */
    cm_code_prologue_init();
    map_exec(code_gen_prologue, sizeof(code_gen_prologue));
}

void cm_cpu_exec_init_core(void)
{
    cpu_gen_init();
    /* Get code cache. */
    cm_code_gen_alloc();
    code_gen_ptr = code_gen_buffer;

#if defined(TARGET_I386)
    optimize_flags_init();
#elif defined(TARGET_ARM)
    arm_translate_init();
#endif
    /* Setup the scheduling for core thread */
    //coremu_init_sched_core();

//ANALYTICAL_INIT
#ifdef ANALYTICAL_MODEL_INSTRUMENT
	core_analytical_model_instrument_flag = 0;
	core_analytical_model_instrument_flag_before_magic_break = 0;
	previous_timing_call = 0;

#ifdef ANALYTICAL_MODEL_STATISTICS
	analytical_user_level_statics = 0;
	analytical_user_instruction_num[cpu_single_env->cpu_index] = 0;
	analytical_kernel_instruction_num[cpu_single_env->cpu_index] = 0;
	analytical_total_instruction_num[cpu_single_env->cpu_index] = 0;
	timing_call_times[cpu_single_env->cpu_index] = 0;
	analytical_max_basic_block_instr_num[cpu_single_env->cpu_index] = 0;
/*
	char *ftmp_file_name = "functional.log";
	char fana_log_file_name[16];
	sprintf(fana_log_file_name, "%s%d", ftmp_file_name, cpu_single_env->cpu_index);
	printf("%s\n", fana_log_file_name);
	const char *fanalytical_log_file_name = (const char *)fana_log_file_name;
	fanalytical_log_file = fopen(fanalytical_log_file_name, "w+");*/
#endif

#ifdef ANALYTICAL_MODEL_FETCH_INSTR
	ana_record_pc = 0;
	ana_record_next_pc = 0;
	memset(fetch_single_instr_bytes, 0, sizeof(fetch_single_instr_bytes));
#endif

#ifdef ANALYTICAL_MODEL_HELPLER_FETCH_INSTR
	ana_helpler_record_pc = 0;
	ana_helpler_record_next_pc = 0;
	ana_fetch_instr_temp_buffer = (ana_instr_struct_t *)malloc(ANALYTICAL_SINGLE_TB_MAX_INSTR_BUFFER_SIZE * sizeof(ana_instr_struct_t));
	//printf("core: %d, ana_fetch_instr_temp_buffer: %llx\n", cpu_single_env->cpu_index, ana_fetch_instr_temp_buffer);
	memset(ana_fetch_instr_temp_buffer, 0, ANALYTICAL_SINGLE_TB_MAX_INSTR_BUFFER_SIZE * sizeof(ana_instr_struct_t));
	ana_single_tb_instr_index = 0;
#endif

#ifdef ANALYTICAL_MODEL_HELPLER_FETCH_MEM
	ana_fetch_mem_temp_buffer =  (ana_mem_struct_t *)malloc(ANALYTICAL_SINGLE_TB_MAX_MEM_BUFFER_SIZE * sizeof(ana_mem_struct_t));
	memset(ana_fetch_mem_temp_buffer, 0, ANALYTICAL_SINGLE_TB_MAX_MEM_BUFFER_SIZE * sizeof(ana_mem_struct_t));
	ana_single_tb_mem_index = 0;
	ana_disas_print = 0;
	ana_cal_print = 0;
	memset(ana_mem_phy_page_log, 0, ANA_MEM_PHY_PAGE_LOG_SIZE * sizeof(target_ulong));
	memset(ana_mem_phy_page_addr_log, 0, ANA_MEM_PHY_PAGE_LOG_SIZE * sizeof(target_ulong));
	ana_mem_phy_page_log_index = 0;
#endif

#ifdef ANALYTICAL_MODEL_TIMING_SHIFT
	ana_tb_temp_buffer = (ana_tb_struct_t *)malloc(sizeof(ana_tb_struct_t));
	memset(ana_tb_temp_buffer, 0, sizeof(ana_tb_struct_t));
	ana_timing_init_core(cpu_single_env->cpu_index);
#endif
#endif

    /* Set up ticks mechanism for every core. */
    cpu_enable_ticks();

    /* Create per core timer. */
    if (cm_init_local_timer_alarm() < 0) {
        coremu_assert(0, "local alarm initialize failed");
    }

#ifdef COREMU_CACHESIM_MODE
    cm_memtrace_init(cpu_single_env->cpu_index);
#endif

#ifdef COREMU_DEBUG_MODE
    cm_wtrigger_buf_init();
#endif
    /* Wait other core to finish initialization. */
    coremu_wait_init();
}
