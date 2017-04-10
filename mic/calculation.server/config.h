#ifndef __CONFIG_H_
#define __CONFIG_H_

#include <inttypes.h>
#include <string.h>
#include <assert.h>
#include "trans_define.h"

typedef unsigned char  byte_t;          /* byte - 8 bits */
typedef unsigned short half_t;          /* half - 16 bits */
typedef unsigned int word_t;          /* word - 32 bits */

#define BLOCK_INSTRUCTION (200)

#define MAX_FETCH (4)
#define MAX_DECODE (4)
#define MAX_DISPATCH (4)
#define MAX_EXECUTE (4)
#define MAX_RETIRE (15)

#define MUL_LATENCY (4)
#define DIV_LATENCY (10)

#define BBV_DIM (16)
#define GATHER_BLOCK_NUM (10000)
#define MAX_MISTAKE (20000)

#define DL1_ASSOC (2) 
#define DL1_SET_BITS (9) 
#define DL1_BLOCK_BITS (6)

#define L2_ASSOC (8)
#define L2_SET_BITS (13)	//modified
#define L2_BLOCK_BITS (6)

#define IWINDOW_WIN_SIZE (64)

/** Predictor config*/
#define BRANCHPRED_PHT_BITS 20
#define BRANCHPRED_EXCEPTION_BITS 15
#define BRANCHPRED_TAG_BITS 15

//#define INORDER_L1_CACHE_LATENCY 3
#define INORDER_L1_CACHE_LATENCY (2)
#define INORDER_L2_CACHE_LATENCY (20)	//modified
#define INORDER_MEMORY_LATENCY (200)

#define INORDER_MISPREDICTION_LATENCY (3)

#define INORDER_MAX_STALL_INST_NUM IWINDOW_WIN_SIZE
#define INORDER_MAX_ESTIMATE_INST_NUM (1 << 12)

#define CORRECT_RETIRE_BUF_SIZE (64)

#define QUEUE_SIZE (5)
#define THRESHOLD (20000)
#define NUM_PROCS (1025)
#define NEW_ENTRY (8000)

#define MAX_TRIFS_TRACE_BUF_BLOCK_SIZE (64)
#define MAX_TRIFS_TRACE_BUF_SIZE (128)
#define MAX_TRIFS_TRACE_TB_INST_NUM (100)
#define TRIFS_TRACE_MEM_SIZE_PER_TB (64)
#define MAX_TRIFS_TRACE_X86_BUF_SIZE (64) 
#define MAX_X86_UOP_NUM (16)

// load-store-queue length
#define CAL_LSQ_STORE_QUEUE_LENGTH 2

// Parameters for Parallelization_by_core, shows how many cores 
// that one thread will simulate
#define trans_core_per_thread 1

// Threshold for empty qemu advance
#define TRANS_empty_advance_threshold 5

//#define slack mechanism parameters
#define TRANS_SLACK_SIZE (20)
#define MAX_OPAL_SLACK (20)

#define QEMU_SI_MAX_SOURCE 3
#define QEMU_SI_MAX_DEST 1

#define QEMU_MAX_CHANGED_REGS 8
#define QEMU_NUM_REGS 256

#define QEMU_NUM_CORES 16
#define QEMU_NUM_LOGICAL_PER_CORE 1

#define QEMU_RET_NORMAL 0
#define QEMU_RET_FAIL -1
#define QEMU_RET_ROLLBACK 10
#define QEMU_RET_FINISH 1

#define WPATH_INITIALIZE 20
#define WPATH_PREDICT_PC 30
#define WPATH_ROLLBACK 40
#define WPATH_TERMINATE 50

#define QEMU_RET_WPATH_INITIALIZE 21
#define QEMU_RET_WPATH_PREDICT_PC 31
#define QEMU_RET_WPATH_ROLLBACK 41
#define QEMU_RET_WPATH_TERMINATE 51

#define QEMU_BRANCH_TAKEN 1

#define QEMU_ADVANCE_EMPRY (1)
#define QEMU_ADVANCE_OK (0)
#define QEMU_ADVANCE_FAIL (-1)
#define QEMU_ADVANCE_TERMINATE (10)

#define NOT_RETIRED (2)
#define FETCH_READY (1)
//#define EMPTY (0)

#define FULL (1)

// Cache simulation

#define TRANS_directory_history_size 4

#define Coherence_buffer_size 256

#define CAT_CACHE_EMPTY 0
#define CAT_CACHE_FULL 1

#define Coherence_load_miss 1
#define Coherence_load_hit 2
#define Coherence_store_miss 3
#define Coherence_store_hit 4
#define Coherence_replace 5
#define Coherence_inv 6

#define Coherence_status_none 0
#define Coherence_status_valid 1
#define Coherence_status_replaced 2

#endif

