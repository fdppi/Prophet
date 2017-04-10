#ifndef __CALCULATION_H
#define __CALCULATION_H


/*------------------------------------------------------------------------*/
/* Includes                                                               */
/*------------------------------------------------------------------------*/

#include <stdio.h>
#include <sys/types.h>
#include <pthread.h>
#include <vector>
#include <map>


#include "yags.h"
#include "config.h"


#ifdef TRANSFORMER_INORDER_CACHE
#include "trans_cache.h"
#endif


/*------------------------------------------------------------------------*/
/* Macro declarations                                                     */
/*------------------------------------------------------------------------*/


/*------------------------------------------------------------------------*/
/* Data type and variable declaration(s)                                                   */
/*------------------------------------------------------------------------*/

#define ANALYTICAL_COREMU_TOTAL_CORE 256
#define ANALYTICAL_INSTRUCTION_FETCH_SINGLE_MAX_LENTGH 16
#define ANALYTICAL_SINGLE_TB_MAX_INSTR_BUFFER_SIZE 128
#define ANALYTICAL_SINGLE_TB_MAX_MEM_BUFFER_SIZE 128

#define ANALYTICAL_MODEL_HELPLER_FETCH_INSTR

#define ANALYTICAL_MODEL_HELPLER_FETCH_MEM
#define ANALYTICAL_MODEL_TIMING_SHIFT
//#define ANALYTICAL_MODEL_TRACE_LOG

#ifdef ANALYTICAL_MODEL_HELPLER_FETCH_INSTR
typedef struct {
	uint64_t ana_x86_virt_pc;
	unsigned char ana_x86_inst[ANALYTICAL_INSTRUCTION_FETCH_SINGLE_MAX_LENTGH];
} ana_instr_struct_t;
#endif

#ifdef ANALYTICAL_MODEL_HELPLER_FETCH_MEM
typedef struct {
	uint64_t ana_x86_virt_pc;
	uint32_t ana_physical_addr;
} ana_mem_struct_t;
#endif

#ifdef ANALYTICAL_MODEL_TIMING_SHIFT
typedef struct {
	uint64_t ana_tb_addr;
	uint64_t ana_eip;
	int ana_tb_mem_num;
} ana_tb_struct_t;
#endif

typedef struct {
    volatile byte_t flag;
} instr_info_buffer_flag_t; 

#define MAX_TRIFS_TRACE_X86_BUF_SIZE 16
typedef struct {
	uint64_t tb_addr;
	uint32_t x86_eip;
	uint32_t tb_mem_num;
	byte_t inst_num;
} x86_tb_instr_buffer_t;

typedef struct {
	byte_t tb_mem_num;
	unsigned char x86_inst[MAX_TRIFS_TRACE_X86_BUF_SIZE];
	uint32_t x86_virt_pc;
} x86_instr_info_buffer_t;

typedef struct {
	uint32_t logical_pc;
	uint32_t m_physical_addr;
} x86_mem_info_buffer_t;

typedef struct instr_info_buffer_s{
    uint64_t logical_pc;
    byte_t  m_source_reg[QEMU_SI_MAX_SOURCE];

    /// functional unit for executing
    int m_futype;

    /// register identifiers for the destination registers
    half_t  m_dest_reg[QEMU_SI_MAX_DEST];
    
    #ifdef TRANSFORMER_MISPREDICTION
    uint64_t branch_taken_npc_pc;
    uint64_t branch_untaken_npc_pc;
    byte_t m_branch_result;
    #endif

    /// memory access size
    //byte_t m_access_size;
    /// physical address for the memory access address
    uint64_t m_physical_addr;

}instr_info_buffer_t;

typedef unsigned long long W64;
typedef signed long long W64s;
typedef unsigned int W32;
typedef signed int W32s;
typedef unsigned short W16;
typedef signed short W16s;
typedef unsigned char byte;
typedef unsigned char W8;
typedef signed char W8s;

struct TransOp {
    // Opcode:
    byte_t opcode;
    // Size shift, extshift
    byte_t size:2; 
    union { byte_t memory_fence_type:2; 
        byte_t extshift:2; 
    } ;
    // Condition codes (for loads/stores, cond = alignment)
    byte_t cond:4, setflags:3, nouserflags:1;
    // Loads and stores:
    byte_t internal:1, locked:1, cachelevel:2, datatype:4;
    // x86 semantics
    byte_t bytes:4, som:1, eom:1, is_sse:1, is_x87:1;
    // Operands
    byte_t rd, ra, rb, rc;
    // Index in basic block
    byte_t bbindex;
    // Misc info
    byte_t unaligned:1, chktype:7;
    // Immediates
    W64s rbimm;
    W64s rcimm;
    W64 riptaken;
    W64 ripseq;
};

enum i_opcode {
    i_add = 0,
    i_addcc,
    i_addc,
    i_addccc,
    i_and,
    i_andcc,
    i_andn,
    i_andncc,
    i_fba,
    i_ba,
    i_fbpa,
    i_bpa,
    i_fbn,
    i_fbpn,
    i_bn,
    i_bpn,
    i_bpne,
    i_bpe,
    i_bpg,
    i_bple,
    i_bpge,
    i_bpl,
    i_bpgu,
    i_bpleu,
    i_bpcc,
    i_bpcs,
    i_bppos,
    i_bpneg,
    i_bpvc,
    i_bpvs,
    i_call,
    i_casa,
    i_casxa,
    i_done,
    i_jmpl,
    i_fabss,
    i_fabsd,
    i_fabsq,
    i_fadds,
    i_faddd,
    i_faddq,
    i_fsubs,
    i_fsubd,
    i_fsubq,
    i_fcmps,
    i_fcmpd,
    i_fcmpq,
    i_fcmpes,
    i_fcmped,
    i_fcmpeq,
    i_fmovs,
    i_fmovd,
    i_fmovq,
    i_fnegs,
    i_fnegd,
    i_fnegq,
    i_fmuls,
    i_fmuld,
    i_fmulq,
    i_fdivs,
    i_fdivd,
    i_fdivq,
    i_fsmuld,
    i_fdmulq,
    i_fsqrts,
    i_fsqrtd,
    i_fsqrtq,
    i_retrn,
    i_brz,
    i_brlez,
    i_brlz,
    i_brnz,
    i_brgz,
    i_brgez,
    i_fbu,
    i_fbg,
    i_fbug,
    i_fbl,
    i_fbul,
    i_fblg,
    i_fbne,
    i_fbe,
    i_fbue,
    i_fbge,
    i_fbuge,
    i_fble,
    i_fbule,
    i_fbo,
    i_fbpu,
    i_fbpg,
    i_fbpug,
    i_fbpl,
    i_fbpul,
    i_fbplg,
    i_fbpne,
    i_fbpe,
    i_fbpue,
    i_fbpge,
    i_fbpuge,
    i_fbple,
    i_fbpule,
    i_fbpo,
    i_bne,
    i_be,
    i_bg,
    i_ble,
    i_bge,
    i_bl,
    i_bgu,
    i_bleu,
    i_bcc,
    i_bcs,
    i_bpos,
    i_bneg,
    i_bvc,
    i_bvs,
    i_fstox,
    i_fdtox,
    i_fqtox,
    i_fstoi,
    i_fdtoi,
    i_fqtoi,
    i_fstod,
    i_fstoq,
    i_fdtos,
    i_fdtoq,
    i_fqtos,
    i_fqtod,
    i_fxtos,
    i_fxtod,
    i_fxtoq,
    i_fitos,
    i_fitod,
    i_fitoq,
    i_fmovfsn,
    i_fmovfsne,
    i_fmovfslg,
    i_fmovfsul,
    i_fmovfsl,
    i_fmovfsug,
    i_fmovfsg,
    i_fmovfsu,
    i_fmovfsa,
    i_fmovfse,
    i_fmovfsue,
    i_fmovfsge,
    i_fmovfsuge,
    i_fmovfsle,
    i_fmovfsule,
    i_fmovfso,
    i_fmovfdn,
    i_fmovfdne,
    i_fmovfdlg,
    i_fmovfdul,
    i_fmovfdl,
    i_fmovfdug,
    i_fmovfdg,
    i_fmovfdu,
    i_fmovfda,
    i_fmovfde,
    i_fmovfdue,
    i_fmovfdge,
    i_fmovfduge,
    i_fmovfdle,
    i_fmovfdule,
    i_fmovfdo,
    i_fmovfqn,
    i_fmovfqne,
    i_fmovfqlg,
    i_fmovfqul,
    i_fmovfql,
    i_fmovfqug,
    i_fmovfqg,
    i_fmovfqu,
    i_fmovfqa,
    i_fmovfqe,
    i_fmovfque,
    i_fmovfqge,
    i_fmovfquge,
    i_fmovfqle,
    i_fmovfqule,
    i_fmovfqo,
    i_fmovsn,
    i_fmovse,
    i_fmovsle,
    i_fmovsl,
    i_fmovsleu,
    i_fmovscs,
    i_fmovsneg,
    i_fmovsvs,
    i_fmovsa,
    i_fmovsne,
    i_fmovsg,
    i_fmovsge,
    i_fmovsgu,
    i_fmovscc,
    i_fmovspos,
    i_fmovsvc,
    i_fmovdn,
    i_fmovde,
    i_fmovdle,
    i_fmovdl,
    i_fmovdleu,
    i_fmovdcs,
    i_fmovdneg,
    i_fmovdvs,
    i_fmovda,
    i_fmovdne,
    i_fmovdg,
    i_fmovdge,
    i_fmovdgu,
    i_fmovdcc,
    i_fmovdpos,
    i_fmovdvc,
    i_fmovqn,
    i_fmovqe,
    i_fmovqle,
    i_fmovql,
    i_fmovqleu,
    i_fmovqcs,
    i_fmovqneg,
    i_fmovqvs,
    i_fmovqa,
    i_fmovqne,
    i_fmovqg,
    i_fmovqge,
    i_fmovqgu,
    i_fmovqcc,
    i_fmovqpos,
    i_fmovqvc,
    i_fmovrsz,
    i_fmovrslez,
    i_fmovrslz,
    i_fmovrsnz,
    i_fmovrsgz,
    i_fmovrsgez,
    i_fmovrdz,
    i_fmovrdlez,
    i_fmovrdlz,
    i_fmovrdnz,
    i_fmovrdgz,
    i_fmovrdgez,
    i_fmovrqz,
    i_fmovrqlez,
    i_fmovrqlz,
    i_fmovrqnz,
    i_fmovrqgz,
    i_fmovrqgez,
    i_mova,
    i_movfa,
    i_movn,
    i_movfn,
    i_movne,
    i_move,
    i_movg,
    i_movle,
    i_movge,
    i_movl,
    i_movgu,
    i_movleu,
    i_movcc,
    i_movcs,
    i_movpos,
    i_movneg,
    i_movvc,
    i_movvs,
    i_movfu,
    i_movfg,
    i_movfug,
    i_movfl,
    i_movful,
    i_movflg,
    i_movfne,
    i_movfe,
    i_movfue,
    i_movfge,
    i_movfuge,
    i_movfle,
    i_movfule,
    i_movfo,
    i_movrz,
    i_movrlez,
    i_movrlz,
    i_movrnz,
    i_movrgz,
    i_movrgez,
    i_ta,
    i_tn,
    i_tne,
    i_te,
    i_tg,
    i_tle,
    i_tge,
    i_tl,
    i_tgu,
    i_tleu,
    i_tcc,
    i_tcs,
    i_tpos,
    i_tneg,
    i_tvc,
    i_tvs,
    i_sub,
    i_subcc,
    i_subc,
    i_subccc,
    i_or,
    i_orcc,
    i_orn,
    i_orncc,
    i_xor,
    i_xorcc,
    i_xnor,
    i_xnorcc,
    i_mulx,
    i_sdivx,
    i_udivx,
    i_sll,
    i_srl,
    i_sra,
    i_sllx,
    i_srlx,
    i_srax,
    i_taddcc,
    i_taddcctv,
    i_tsubcc,
    i_tsubcctv,
    i_udiv,
    i_sdiv,
    i_umul,
    i_smul,
    i_udivcc,
    i_sdivcc,
    i_umulcc,
    i_smulcc,
    i_mulscc,
    i_popc,
    i_flush,
    i_flushw,
    i_rd,
    i_rdcc,
    i_wr,
    i_wrcc,
    i_save,
    i_restore,
    i_saved,
    i_restored,
    i_sethi,
    i_ldf,
    i_lddf,
    i_ldqf,
    i_ldfa,
    i_lddfa,
    i_ldblk,
    i_ldqfa,
    i_ldsb,
    i_ldsh,
    i_ldsw,
    i_ldub,
    i_lduh,
    i_lduw,
    i_ldx,
    i_ldd,
    i_ldsba,
    i_ldsha,
    i_ldswa,
    i_lduba,
    i_lduha,
    i_lduwa,
    i_ldxa,
    i_ldda,
    i_ldqa,
    i_stf,
    i_stdf,
    i_stqf,
    i_stb,
    i_sth,
    i_stw,
    i_stx,
    i_std,
    i_stfa,
    i_stdfa,
    i_stblk,
    i_stqfa,
    i_stba,
    i_stha,
    i_stwa,
    i_stxa,
    i_stda,
    i_ldstub,
    i_ldstuba,
    i_prefetch,
    i_prefetcha,
    i_swap,
    i_swapa,
    i_ldfsr,
    i_ldxfsr,
    i_stfsr,
    i_stxfsr,
    i__trap,  /* ?? not a real instr */
    i_impdep1,
    i_impdep2,
    i_membar,
    i_stbar,
    i_cmp,
    i_jmp,
    i_mov,
    i_nop,
    i_not,
    i_rdpr,
    i_wrpr,
    i_faligndata,
    i_alignaddr,
    i_alignaddrl,
    i_fzero,
    i_fzeros,
    i_fsrc1,
    i_retry,
    i_fcmple16,
    i_fcmpne16,
    i_fcmple32,
    i_fcmpne32,
    i_fcmpgt16,
    i_fcmpeq16,
    i_fcmpgt32,
    i_fcmpeq32,
    i_bshuffle,
    i_bmask,
    i_mop,
    i_ill,
    i_maxcount
};

enum {
  OP_nop,          // 0
  OP_mov,
  // Logical
  OP_and,
  OP_andnot,
  OP_xor,
  OP_or,
  OP_nand,
  OP_ornot,
  OP_eqv,
  OP_nor,
  // Mask, insert or extract bytes
  OP_maskb,        // 10
  // Add and subtract
  OP_add,
  OP_sub,
  OP_adda,
  OP_suba,
  OP_addm,
  OP_subm,
  // Condition code logical ops

  OP_andcc,
  OP_orcc,
  OP_xorcc,
  OP_ornotcc,      // 20
  // Condition code movement and merging
  OP_movccr,
  OP_movrcc,
  OP_collcc,
  // Simple shifting (restricted to small immediate 1..8)
  OP_shls,
  OP_shrs,
  OP_bswap,
  OP_sars,
  // Bit testing
  OP_bt,
  OP_bts,
  OP_btr,          // 30
  OP_btc,
  // Set and select
  OP_set,
  OP_set_sub,
  OP_set_and,
  OP_sel,
  // Branches
  OP_br,
  OP_br_sub,
  OP_br_and,
  OP_jmp,
  OP_bru,          // 40
  OP_jmpp,
  OP_brp,
  // Checks
  OP_chk,
  OP_chk_sub,
  OP_chk_and,
  // Loads and stores
  OP_ld,
  OP_ldx,
  OP_ld_pre,
  OP_st,
  OP_mf,           // 50
  // Shifts, rotates and complex masking
  OP_shl,
  OP_shr,
  OP_mask,
  OP_sar,
  OP_rotl,
  OP_rotr,
  OP_rotcl,
  OP_rotcr,
  // Multiplication
  OP_mull,
  OP_mulh,         // 60
  OP_mulhu,
  // Bit scans
  OP_ctz,
  OP_clz,
  OP_ctpop,
  OP_permb,
  // Floating point
  OP_addf,
  OP_subf,
  OP_mulf,
  OP_maddf,
  OP_msubf,        // 70
  OP_divf,
  OP_sqrtf,
  OP_rcpf,
  OP_rsqrtf,
  OP_minf,
  OP_maxf,
  OP_cmpf,
  OP_cmpccf,
  OP_permf,
  OP_cvtf_i2s_ins, // 80
  OP_cvtf_i2s_p,
  OP_cvtf_i2d_lo,
  OP_cvtf_i2d_hi,
  OP_cvtf_q2s_ins,
  OP_cvtf_q2d,
  OP_cvtf_s2i,
  OP_cvtf_s2q,
  OP_cvtf_s2i_p,
  OP_cvtf_d2i,
  OP_cvtf_d2q,     // 90
  OP_cvtf_d2i_p,
  OP_cvtf_d2s_ins,
  OP_cvtf_d2s_p,
  OP_cvtf_s2d_lo,
  OP_cvtf_s2d_hi,
  // Multiplication
  OP_divr,
  OP_divq,
  OP_divru,
  OP_divqu,
#if 1
  OP_mulhl,
#endif
#if 1
  // Vector integer uops
  // size defines element size: 00 = byte, 01 = W16, 10 = W32, 11 = W64 (same as normal ops)
  OP_vadd,
  OP_vsub,
  OP_vadd_us,
  OP_vsub_us,
  OP_vadd_ss,
  OP_vsub_ss,
  OP_vshl,
  OP_vshr,
  OP_vbt, // bit test vector (e.g. pack bit 7 of 8 bytes into 8-bit output, for pmovmskb)
  OP_vsar,
  OP_vavg,
  OP_vcmp,
  OP_vmin,
  OP_vmax,
  OP_vmin_s,
  OP_vmax_s,
  OP_vmull,
  OP_vmulh,
  OP_vmulhu,
  OP_vmaddp,
  OP_vsad,
  OP_vpack_us,
  OP_vpack_ss,
#endif
  OP_MAX_OPCODE,   // 100
};

/** classifications of different types of instructions.
 *  this def'n should migrate here if definitions.h ever dies!
 */
enum dyn_execute_type_t {
    DYN_NONE = 0,              // not identifyable
    DYN_EXECUTE,               // integer operation
    DYN_CONTROL,               // branch, call, jump
    DYN_LOAD,                  // load
    DYN_STORE,                 // store
    DYN_PREFETCH,              // prefetch memory operation
    DYN_ATOMIC,                // atomic memory operation (atomic swap, etc)
    DYN_NUM_EXECUTE_TYPES
};

/** classifications of different types of functional unit dependencies,
 *  intalu, floatmult, etc.
 */

enum fu_type_t {
    FU_NONE = 0,             // inst does not use a functional unit
    FU_INTALU,               // integer ALU
    FU_INTMULT,              // integer multiplier
    FU_INTDIV,               // integer divider
    FU_BRANCH,               // compare / branch unit
    FU_FLOATADD,             // floating point adder/subtractor
    FU_FLOATCMP,             // floating point comparator
    FU_FLOATCVT,             // floating point<->integer converter
    FU_FLOATMULT,            // floating point multiplier
    FU_FLOATDIV,             // floating point divider
    FU_FLOATSQRT,            // floating point square root
    FU_RDPORT,               // memory read port
    FU_WRPORT,               // memory write port
    FU_NUM_FU_TYPES          // total functional unit classes
};

enum branch_type_t {
    BRANCH_NONE = 0,           // not a branch
    BRANCH_UNCOND,             // unconditional branch
    BRANCH_COND,               // conditional branch
    BRANCH_PCOND,              // predicated conditional branch
    BRANCH_CALL,               // call
    BRANCH_RETURN,             // return from call (jmp addr, %g0)
    BRANCH_INDIRECT,           // indirect call    (jmpl)
    BRANCH_CWP,                // current window pointer update
    BRANCH_TRAP_RETURN,        // return from trap
    BRANCH_TRAP,               // trap ? indirect jump ??? incorrect?
    BRANCH_PRIV,               // explicit privilege  level change
    BRANCH_NUM_BRANCH_TYPES
};

typedef struct {
    uint32_t uops_opcode;
    uint32_t gems_opcode;
    uint32_t gems_type;
    uint32_t gems_futype;
} uops_map_info_t;

static const uops_map_info_t uops_gems_map_table[OP_MAX_OPCODE] = {
    {OP_nop,            i_nop,          DYN_EXECUTE,            FU_NONE},
    {OP_mov,            i_mov,          DYN_EXECUTE,            FU_NONE},
    // Logical
    {OP_and,            i_and,          DYN_EXECUTE,            FU_INTALU},
    {OP_andnot,         i_andn,         DYN_EXECUTE,            FU_INTALU},
    {OP_xor,            i_xor,          DYN_EXECUTE,            FU_INTALU},
    {OP_or,             i_or,           DYN_EXECUTE,            FU_INTALU},
    {OP_nand,           i_andn,         DYN_EXECUTE,            FU_INTALU},
    {OP_ornot,          i_orn,          DYN_EXECUTE,            FU_INTALU},
    {OP_eqv,            i_cmp,          DYN_EXECUTE,            FU_INTALU},
    {OP_nor,            i_xnor,         DYN_EXECUTE,            FU_INTALU},
    // Mask, insert or extract bytes
    {OP_maskb,          i_bmask,        DYN_EXECUTE,            FU_INTALU},

    // Add and subtract
    {OP_add,            i_add,          DYN_EXECUTE,            FU_INTALU},
    {OP_sub,            i_sub,          DYN_EXECUTE,            FU_INTALU},
    {OP_adda,           i_add,          DYN_EXECUTE,            FU_INTALU},
    {OP_suba,           i_sub,          DYN_EXECUTE,            FU_INTALU},
    {OP_addm,           i_add,          DYN_EXECUTE,            FU_INTALU},
    {OP_subm,           i_sub,          DYN_EXECUTE,            FU_INTALU},
    // Condition code logical ops
    {OP_andcc,          i_andcc,        DYN_EXECUTE,            FU_INTALU},
    {OP_orcc,           i_orcc,         DYN_EXECUTE,            FU_INTALU},
    {OP_xorcc,          i_xorcc,        DYN_EXECUTE,            FU_INTALU},
    {OP_ornotcc,        i_orncc,        DYN_EXECUTE,            FU_INTALU},

    // Condition code movement and merging
    {OP_movccr,         i_movcc,        DYN_EXECUTE,            FU_INTALU},
    {OP_movrcc,         i_movcc,        DYN_EXECUTE,            FU_INTALU},
    {OP_collcc,         i_movcc,        DYN_EXECUTE,            FU_INTALU},
    // Simple shifting (restricted to small immediate 1..8)
    {OP_shls,           i_sll,          DYN_EXECUTE,            FU_INTALU},
    {OP_shrs,           i_srl,          DYN_EXECUTE,            FU_INTALU},
    {OP_bswap,          i_orcc,         DYN_EXECUTE,            FU_INTALU},
    {OP_sars,           i_sra,          DYN_EXECUTE,            FU_INTALU},
    // Bit testing
    {OP_bt,             i_tcc,          DYN_EXECUTE,            FU_INTALU},
    {OP_bts,            i_tcc,          DYN_EXECUTE,            FU_INTALU},
    {OP_btr,            i_tcc,          DYN_EXECUTE,            FU_INTALU},

    {OP_btc,            i_tcc,          DYN_EXECUTE,            FU_INTALU},
    // Set and select
    {OP_set,            i_mov,          DYN_EXECUTE,            FU_INTALU},
    {OP_set_sub,        i_sub,          DYN_EXECUTE,            FU_INTALU},
    {OP_set_and,        i_and,          DYN_EXECUTE,            FU_INTALU},
    {OP_sel,            i_mov,          DYN_EXECUTE,            FU_INTALU},
        //{OP_sel_cmp,        i_cmp,          DYN_EXECUTE,            FU_INTALU},
    // Branches
    {OP_br,             i_brz,          DYN_CONTROL,            FU_BRANCH},
    {OP_br_sub,         i_brz,          DYN_CONTROL,            FU_BRANCH},
    {OP_br_and,         i_brz,          DYN_CONTROL,            FU_BRANCH},
    {OP_jmp,            i_jmpl,         DYN_CONTROL,            FU_NONE},
    {OP_bru,            i_brz,          DYN_CONTROL,            FU_BRANCH},

    {OP_jmpp,           i_jmpl,         DYN_CONTROL,            FU_NONE},
    {OP_brp,            i_brz,          DYN_CONTROL,            FU_BRANCH},
    // Checks
    {OP_chk,            i_tcc,          DYN_EXECUTE,            FU_INTALU},
    {OP_chk_sub,        i_tcc,          DYN_EXECUTE,            FU_INTALU},
    {OP_chk_and,        i_tcc,          DYN_EXECUTE,            FU_INTALU},
    // Loads and stores
    {OP_ld,             i_ldx,          DYN_LOAD,               FU_RDPORT},
    {OP_ldx,            i_ldx,          DYN_LOAD,               FU_RDPORT},
    {OP_ld_pre,         i_ldx,          DYN_LOAD,               FU_RDPORT},
    {OP_st,             i_stx,          DYN_STORE,              FU_WRPORT},
    {OP_mf,             i_stx,          DYN_STORE,              FU_WRPORT},

    // Shifts, rotates and complex masking
    {OP_shl,            i_sll,          DYN_EXECUTE,            FU_INTALU},
    {OP_shr,            i_srl,          DYN_EXECUTE,            FU_INTALU},
    {OP_mask,           i_bmask,        DYN_EXECUTE,            FU_INTALU},
    {OP_sar,            i_sra,          DYN_EXECUTE,            FU_INTALU},
    {OP_rotl,           i_sll,          DYN_EXECUTE,            FU_INTALU},
    {OP_rotr,           i_srl,          DYN_EXECUTE,            FU_INTALU},
    {OP_rotcl,          i_sll,          DYN_EXECUTE,            FU_INTALU},
    {OP_rotcr,          i_srl,          DYN_EXECUTE,            FU_INTALU},
    // Multiplication
    {OP_mull,           i_fmuld,        DYN_EXECUTE,            FU_FLOATMULT},
    {OP_mulh,           i_fmuld,        DYN_EXECUTE,            FU_FLOATMULT},

    {OP_mulhu,          i_fmuld,        DYN_EXECUTE,            FU_FLOATMULT},
        //{OP_mulhl,          i_mulx,         DYN_EXECUTE,            FU_INTMULT},
    // Bit scans
    {OP_ctz,            i_fmuld,        DYN_EXECUTE,            FU_FLOATMULT},
    {OP_clz,            i_fmuld,        DYN_EXECUTE,            FU_FLOATMULT},
    {OP_ctpop,          i_fmuld,        DYN_EXECUTE,            FU_FLOATMULT},
    {OP_permb,          i_fmuld,        DYN_EXECUTE,            FU_FLOATMULT},
    // Integer divide and remainder step
        //{OP_div,            i_udiv,         DYN_EXECUTE,            FU_INTDIV},
        //{OP_rem,            i_udiv,         DYN_EXECUTE,            FU_INTDIV},
        //{OP_divs,           i_udiv,         DYN_EXECUTE,            FU_INTDIV},
        //{OP_rems,           i_udiv,         DYN_EXECUTE,            FU_INTDIV},
    // Minimum and maximum
        //{OP_min,            i_cmp,          DYN_EXECUTE,            FU_INTALU},
        //{OP_max,            i_cmp,          DYN_EXECUTE,            FU_INTALU},
        //{OP_min_s,          i_cmp,          DYN_EXECUTE,            FU_INTALU},
        //{OP_max_s,          i_cmp,          DYN_EXECUTE,            FU_INTALU},
    // Floating point
        //{OP_fadd,           i_faddd,        DYN_EXECUTE,            FU_FLOATADD},
    {OP_addf,           i_faddd,        DYN_EXECUTE,            FU_FLOATADD},
        //{OP_fsub,           i_fsubd,        DYN_EXECUTE,            FU_FLOATADD},
    {OP_subf,           i_fsubd,        DYN_EXECUTE,            FU_FLOATADD},
        //{OP_fmul,           i_fmuld,        DYN_EXECUTE,            FU_FLOATMULT},
    {OP_mulf,           i_fmuld,        DYN_EXECUTE,            FU_FLOATMULT},
        //{OP_fmadd,          i_fmuld,        DYN_EXECUTE,            FU_FLOATMULT},
    {OP_maddf,          i_fmuld,        DYN_EXECUTE,            FU_FLOATMULT},
        //{OP_fmsub,          i_fmuld,        DYN_EXECUTE,            FU_FLOATMULT},
    {OP_msubf,          i_fmuld,        DYN_EXECUTE,            FU_FLOATMULT},

        //{OP_fmsubr,         i_fmuld,        DYN_EXECUTE,            FU_FLOATMULT},
        //{OP_fdiv,           i_fdivd,        DYN_EXECUTE,            FU_FLOATDIV},
    {OP_divf,           i_fdivd,        DYN_EXECUTE,            FU_FLOATDIV},
        //{OP_fsqrt,          i_fsqrtd,       DYN_EXECUTE,            FU_FLOATSQRT},
    {OP_sqrtf,          i_fsqrtd,       DYN_EXECUTE,            FU_FLOATSQRT},
        //{OP_frcp,           i_fsqrtd,       DYN_EXECUTE,            FU_FLOATSQRT},
    {OP_rcpf,           i_fsqrtd,       DYN_EXECUTE,            FU_FLOATSQRT},
        //{OP_frsqrt,         i_fsqrtd,       DYN_EXECUTE,            FU_FLOATSQRT},
    {OP_rsqrtf,         i_fsqrtd,       DYN_EXECUTE,            FU_FLOATSQRT},
        //{OP_fmin,           i_fcmpd,        DYN_EXECUTE,            FU_FLOATCMP},
    {OP_minf,           i_fcmpd,        DYN_EXECUTE,            FU_FLOATCMP},
        //{OP_fmax,           i_fcmpd,        DYN_EXECUTE,            FU_FLOATCMP},
    {OP_maxf,           i_fcmpd,        DYN_EXECUTE,            FU_FLOATCMP},
        //{OP_fcmp,           i_fcmpd,        DYN_EXECUTE,            FU_FLOATCMP},
    {OP_cmpf,           i_fcmpd,        DYN_EXECUTE,            FU_FLOATCMP},
        //{OP_fcmpcc,         i_fcmpd,        DYN_EXECUTE,            FU_FLOATCMP},
    {OP_cmpccf,         i_fcmpd,        DYN_EXECUTE,            FU_FLOATCMP},
    {OP_permf,          i_fcmpd,        DYN_EXECUTE,            FU_FLOATCMP},
        //{OP_fcvt_i2s_ins,   i_fitos,        DYN_EXECUTE,            FU_FLOATCVT},
    {OP_cvtf_i2s_ins,   i_fitos,        DYN_EXECUTE,            FU_FLOATCVT},

        //{OP_fcvt_i2s_p,     i_fitos,        DYN_EXECUTE,            FU_FLOATCVT},
    {OP_cvtf_i2s_p,     i_fitos,        DYN_EXECUTE,            FU_FLOATCVT},
        //{OP_fcvt_i2d_lo,    i_fitod,        DYN_EXECUTE,            FU_FLOATCVT},
    {OP_cvtf_i2d_lo,    i_fitod,        DYN_EXECUTE,            FU_FLOATCVT},
        //{OP_fcvt_i2d_hi,    i_fitos,        DYN_EXECUTE,            FU_FLOATCVT},
    {OP_cvtf_i2d_hi,    i_fitos,        DYN_EXECUTE,            FU_FLOATCVT},
        //{OP_fcvt_q2s_ins,   i_fqtos,        DYN_EXECUTE,            FU_FLOATCVT},
    {OP_cvtf_q2s_ins,   i_fqtos,        DYN_EXECUTE,            FU_FLOATCVT},
        //{OP_fcvt_q2d,       i_fqtod,        DYN_EXECUTE,            FU_FLOATCVT},
    {OP_cvtf_q2d,       i_fqtod,        DYN_EXECUTE,            FU_FLOATCVT},
        //{OP_fcvt_s2i,       i_fstoi,        DYN_EXECUTE,            FU_FLOATCVT},
    {OP_cvtf_s2i,       i_fstoi,        DYN_EXECUTE,            FU_FLOATCVT},
        //{OP_fcvt_s2q,       i_fstoq,        DYN_EXECUTE,            FU_FLOATCVT},
    {OP_cvtf_s2q,       i_fstoq,        DYN_EXECUTE,            FU_FLOATCVT},
        //{OP_fcvt_s2i_p,     i_fstoi,        DYN_EXECUTE,            FU_FLOATCVT},
    {OP_cvtf_s2i_p,     i_fstoi,        DYN_EXECUTE,            FU_FLOATCVT},
        //{OP_fcvt_d2i,       i_fdtoi,        DYN_EXECUTE,            FU_FLOATCVT},
    {OP_cvtf_d2i,       i_fdtoi,        DYN_EXECUTE,            FU_FLOATCVT},
        //{OP_fcvt_d2q,       i_fdtoq,        DYN_EXECUTE,            FU_FLOATCVT},
    {OP_cvtf_d2q,       i_fdtoq,        DYN_EXECUTE,            FU_FLOATCVT},

        //{OP_fcvt_d2i_p,     i_fdtoi,        DYN_EXECUTE,            FU_FLOATCVT},
    {OP_cvtf_d2i_p,     i_fdtoi,        DYN_EXECUTE,            FU_FLOATCVT},
        //{OP_fcvt_d2s_ins,   i_fdtos,        DYN_EXECUTE,            FU_FLOATCVT},
    {OP_cvtf_d2s_ins,   i_fdtos,        DYN_EXECUTE,            FU_FLOATCVT},
        //{OP_fcvt_d2s_p,     i_fdtos,        DYN_EXECUTE,            FU_FLOATCVT},
    {OP_cvtf_d2s_p,     i_fdtos,        DYN_EXECUTE,            FU_FLOATCVT},
        //{OP_fcvt_s2d_lo,    i_fstod,        DYN_EXECUTE,            FU_FLOATCVT},
    {OP_cvtf_s2d_lo,    i_fstod,        DYN_EXECUTE,            FU_FLOATCVT},
        //{OP_fcvt_s2d_hi,    i_fstod,        DYN_EXECUTE,            FU_FLOATCVT},
    {OP_cvtf_s2d_hi,    i_fstod,        DYN_EXECUTE,            FU_FLOATCVT},
    {OP_divr,           i_udiv,         DYN_EXECUTE,            FU_INTDIV},
    {OP_divq,           i_udiv,         DYN_EXECUTE,            FU_INTDIV},
    {OP_divru,          i_udiv,         DYN_EXECUTE,            FU_INTDIV},
    {OP_divqu,          i_udiv,         DYN_EXECUTE,            FU_INTDIV},

    {OP_mulhl,          i_fmuld,        DYN_EXECUTE,            FU_FLOATMULT},

    // Vector integer uops
    // size defines element size: 00 = byte, 01 = W1 10 = W3 11 = W64 (same as normal ops)
    {OP_vadd,           i_add,          DYN_EXECUTE,            FU_INTALU},
    {OP_vsub,           i_sub,          DYN_EXECUTE,            FU_INTALU},
    {OP_vadd_us,        i_add,          DYN_EXECUTE,            FU_INTALU},
    {OP_vsub_us,        i_sub,          DYN_EXECUTE,            FU_INTALU},
    {OP_vadd_ss,        i_add,          DYN_EXECUTE,            FU_INTALU},
    {OP_vsub_ss,        i_sub,          DYN_EXECUTE,            FU_INTALU},
    {OP_vshl,           i_sll,          DYN_EXECUTE,            FU_INTALU},
    {OP_vshr,           i_srl,          DYN_EXECUTE,            FU_INTALU},
    // bit test vector (e.g. pack bit 7 of 8 bytes into 8-bit output, for pmovmskb)
    {OP_vbt,            i_tcc,          DYN_EXECUTE,            FU_INTALU},
    {OP_vsar,           i_sra,          DYN_EXECUTE,            FU_INTALU},
    {OP_vavg,           i_add,          DYN_EXECUTE,            FU_INTALU},
    {OP_vcmp,           i_cmp,          DYN_EXECUTE,            FU_INTALU},
    {OP_vmin,           i_cmp,          DYN_EXECUTE,            FU_INTALU},
    {OP_vmax,           i_cmp,          DYN_EXECUTE,            FU_INTALU},
    {OP_vmin_s,         i_cmp,          DYN_EXECUTE,            FU_INTALU},
    {OP_vmax_s,         i_cmp,          DYN_EXECUTE,            FU_INTALU},
    {OP_vmull,          i_fmuld,        DYN_EXECUTE,            FU_FLOATMULT},
    {OP_vmulh,          i_fmuld,        DYN_EXECUTE,            FU_FLOATMULT},
    {OP_vmulhu,         i_fmuld,        DYN_EXECUTE,            FU_FLOATMULT},
    {OP_vmaddp,         i_fmuld,        DYN_EXECUTE,            FU_FLOATADD},
    {OP_vsad,           i_fmuld,        DYN_EXECUTE,            FU_FLOATADD},
    {OP_vpack_us,       i_add,          DYN_EXECUTE,            FU_INTALU},
    {OP_vpack_ss,       i_add,          DYN_EXECUTE,            FU_INTALU},
};


struct inorder_retire_info
{
    int inorder_uop_sum;
    int inorder_last_pc;
    int inorder_last_fu_type;
    int inorder_skip_cycle;
};

#define MAX_TRIFS_TRACE_UOPS_BUF_SIZE (4 * 1024 * 1024)
#define MAX_TRIFS_TRACE_BUF_SIZE 128

// Added by WHJ 2013.06.25 LSQ
//WHJ lsq

typedef struct cal_lsq_store_queue_s{
    uint64_t store_address;
    uint64_t store_finish_cycle;
} cal_lsq_store_queue_t;


/*------------------------------------------------------------------------*/
/* Function declaration(s)                                                   */
/*------------------------------------------------------------------------*/


void transformer_inorder_timing();
int transformer_inorder_qemu_function(int first_phy_proc);
void * transformer_inorder_qemu_function_thread (void * arg);
void * transformer_inorder_timing_thread (void * arg);
void * transformer_inorder_cache_thread (void * arg);
#ifdef PARALLELIZED_TIMING_BY_CORE
void * transformer_inorder_timing_core_thread_simple(void * arg);
#endif


//#define MAX_TRIFS_TRACE_UOPS_BUF_SIZE (1024)



/*------------------------------------------------------------------------*/
/* Class declaration(s)                                                   */
/*------------------------------------------------------------------------*/

#ifndef CODE_CACHE_H
#define CODE_CACHE_H

using namespace std;

class code_cache{
public:
    code_cache(){};

    vector<instr_info_buffer_t> uops;
    int tb_mem_num;
};

#endif

class pseq_t {
public:
	int m_id;	
	uint64_t m_local_cycles;
	uint64_t m_local_instr_count;
	int m_inorder_fetch_rob_count;
	int tb_fetch_end;
	int fetch_return_ready;
	int timing_call_times_core;

#ifdef ORDER_VIOLATION_TEST
	int ALU_start_cycle[13];
	int ALU_finish_cycle[13];
	int order_violation_rollback_num;
	int order_violation_num;
#endif


	int CONFIG_ALU_MAPPING[13];

	int CONFIG_NUM_ALUS[13];

	int CONFIG_ALU_LATENCY[13];

	FILE *analytical_log_file;

	//uint64_t calculation_wait_num;
	//uint64_t temp_tb_mem_num;
	//uint64_t total_tb_mem_num_dias;
	//uint64_t total_tb_mem_num_plus_dias;
	int inorder_dispatch_inst_count;
	int dispatch_reserve;

	//instr_info_buffer_flag_t *minqh_flag_buffer;
	//int minqh_flag_buffer_index;
	//int minqh_mem_flag_buffer_index;
#ifdef ANALYTICAL_MODEL_TIMING_SHIFT
	ana_tb_struct_t * correct_qemu_tb_instr_buf;
#endif
	ana_instr_struct_t * correct_qemu_instr_info_buf;
	ana_mem_struct_t * correct_qemu_mem_info_buf;

	//int correct_tb_fetch_ptr;
	int correct_timing_ptr;
	int correct_timing_fetch_ptr;
	int correct_timing_release_ptr;
	int correct_mem_fetch_ptr;
	uint64_t same_logical_pc;
	uint64_t same_physical_addr;
	uint32_t same_access_size;
	uint64_t mem_zero_num;
	uint64_t not_cached_num;
	//uint64_t mem_num;
	//uint64_t tcg_mem_num;
	//int print_flag;
	//int tb_has_mem;
	int tb_mem_num;
	int tb_mem_num_index;
	uint64_t step;

	int tb_num_uops;
	int uops_index;
	//int code_cache_found;
	//int tb_start;
	//int tb_end;
	uint64_t x86_eip;
	int tb_uop_end_return_index;
	map< uint64_t, code_cache* > bb_cache_map;
	code_cache *temp_cache;
	int tb_last_inst;
	int last_tb_fetch_num;
	//instr_info_buffer_t *cur_uop;
	//uint64_t cormu_memory_empty_conflict_num;
	
	struct TransOp trifs_trace_uops_buf[512];
	int trifs_trace_num_uops;
	//instr_info_buffer_t *pseq_trifs_trace_uops_buf;
    	//instr_info_buffer_t *pseq_trifs_trace_uops_ptr;

	instr_info_buffer_t trifs_trace_uops_global_buf[MAX_TRIFS_TRACE_UOPS_BUF_SIZE];
	//int trifs_trace_tb_mem_num[MAX_TRIFS_TRACE_UOPS_BUF_SIZE];
	int trifs_trace_uops_global_index;
	int trifs_trace_uops_tb_fetch_ptr;
	x86_mem_info_buffer_t trifs_trace_mem_global_buf[512];
	x86_mem_info_buffer_t *trifs_trace_mem_buf;
	ana_mem_struct_t *trifs_trace_mem_ptr;
	x86_mem_info_buffer_t *trifs_trace_mem_uop_fetch;
	//int mem_tb_end;
	//int mem_tb_start;
	
	int fetch_reserve;

	uint64_t m_inorder_register_finish_cycle[QEMU_NUM_REGS];

	byte_t m_inorder_execute_inst_width[INORDER_MAX_ESTIMATE_INST_NUM];
	byte_t m_inorder_function_unit_width[INORDER_MAX_ESTIMATE_INST_NUM][FU_NUM_FU_TYPES];

	uint64_t *m_inorder_retire_inst_cycle; // size CORRECT BUF SIZE
	int *m_next_pc; // size CORRECT BUF SIZE
	int *m_next_fu_type;
	struct inorder_retire_info *m_inorder_x86_retire_info;

	//WHJ
	cal_lsq_store_queue_t cal_lsq_store_queue_0;
	cal_lsq_store_queue_t cal_lsq_store_queue_1;
	int cal_lsq_store_queue_ptr;

	#ifdef TRANSFORMER_USE_CACHE_COHERENCE_buffer
	// coherence buffer
	volatile coherence_buffer_t *m_coherence_buffer;
	volatile long long *L3_hit_increase_back;
	int m_coherence_buffer_write_ptr;
	#endif

#ifdef TRANSFORMER_INORDER_CACHE
	// private cache
	int CAT_set_bit;
	int CAT_set_size;
	long long CAT_set_mask;
	int CAT_assoc;
	int CAT_block_bit;
	int CAT_block_size;
	int CAT_block_mask;
	int CAT_L2_set_bit;
	int CAT_L2_set_size;
	long long CAT_L2_set_mask;
	int CAT_L2_assoc;
	int CAT_L3_set_bit;
	int CAT_L3_set_size;
	long long CAT_L3_set_mask;
	int CAT_L3_assoc;

	CAT_pseudo_cache_set_t *CAT_pseudo_core_cache;
	CAT_pseudo_cache_set_t *CAT_pseudo_core_L2_cache;

	CAT_pseudo_cache_set_t *CAT_pseudo_L3_cache;
#endif

	// analysis count parameters
	uint64_t inorder_wait_num;
	uint64_t inorder_l1_icache_hit_count;
	uint64_t inorder_l1_icache_miss_count;
	uint64_t inorder_l1_dcache_hit_count;
	uint64_t inorder_l1_dcache_miss_count;
	uint64_t inorder_l2_cache_hit_count;
	uint64_t inorder_l2_cache_miss_count;
	uint64_t inorder_l3_cache_hit_count;
	uint64_t inorder_l3_cache_miss_count;
	uint64_t inorder_branch_count;
	uint64_t inorder_branch_misprediction_count;

#ifdef TRANSFORMER_INORDER_BRANCH
	/** predictor */
	yags_t *m_predictor;
	/** speculative predictor state -- */
	predictor_state_t * m_spec_bpred;
#endif

//#ifdef ANALYTICAL_MODEL_TRACE_LOG
#define ttrifs_trace_log(...) \
    do{ \
        if (analytical_log_file) { \
            fprintf(analytical_log_file, ## __VA_ARGS__); \
            fflush(analytical_log_file); \
        } \
    }while (0);
//#endif

	pseq_t( int id );
	~pseq_t();
	bool m_calculate(int inst_num);
	void log_print_to_file(int inst_num);
	void code_cache_insert(int inst_num, uint64_t tb_addr);
	bool TRANS_previous_code_cache_lookup(int inst_num);
	bool TRANS_inorder_simpleLookupInstruction(instr_info_buffer_t * entry, int inst_num);
	bool trifs_trace_translate_uops(struct TransOp* uop, instr_info_buffer_t *instr_info, uint64_t trifs_trace_virt_pc);
	bool m_write_coherence_buffer(long long address, int type, long long cycle, int in_L3);
	bool trifs_trace_translate_x86_inst_reset();
#ifdef TRANSFORMER_INORDER_CACHE
	bool CAT_pseudo_line_insert(long long address, long long cycle);
	bool CAT_pseudo_L2_line_insert(long long address, long long cycle);
	bool CAT_pseudo_L3_line_insert(long long address, long long cycle);
	bool CAT_pseudo_L3_tag_search(long long address);
	 /** convert the address 'a' to a location (set) in cache */
	inline unsigned int Set(long long a) { 
		return (CAT_set_mask & (long long) (a >> CAT_block_bit));  
	}

	  /** convert the address 'a' to the block address, not shifted */
	inline long long   BlockAddress(long long a) {
		return (a & ~(long long)CAT_block_mask); 
	}

	 /** convert the address 'a' to a location (set) in L3 cache */
	inline unsigned int L3_Set(long long a) { 
	 	return (CAT_L3_set_mask & (long long) (a >> CAT_block_bit));	
	}
#endif
};


#endif  /* __CALCULATION_H s*/

