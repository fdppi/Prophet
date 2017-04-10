/*
 * =====================================================================================
 *
 *       Filename:  translate_inline.h
 *
 *    Description:  Analytical model instrumentation code file
 *
 *        Version:  1.0
 *        Created:  02/17/2014 12:01:28 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (), 
 *        Company:  
 *
 * =====================================================================================
 */

static inline void func_analytical_single_instr_fetch(target_ulong ana_pc_start, target_ulong ana_pc_end){
	int single_instr_byte_index = 0;
	target_ulong ana_fetch_pc = ana_pc_start;
	for(;ana_fetch_pc < ana_pc_end; ana_fetch_pc++){
		fetch_single_instr_bytes[single_instr_byte_index] = ldub_code(ana_fetch_pc);
		single_instr_byte_index++;
	}
	for(;single_instr_byte_index < ANALYTICAL_INSTRUCTION_FETCH_SINGLE_MAX_LENTGH; single_instr_byte_index++){
		fetch_single_instr_bytes[single_instr_byte_index] = 0;
		single_instr_byte_index++;
	}
}