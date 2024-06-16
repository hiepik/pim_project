#include "./pim_unit.h"

namespace dramsim3 {


PimUnit::PimUnit(Config& config, int id) : pim_id(id), config_(config) {
	PPC = 0;
	LC = 0;
	operand_cache = 0;

	// initialize Cache's
	CACHE_ = (unit_t*)malloc(CACHE_SIZE);
	// initialize SRF
	SRF_ = (unit_t*)malloc(SRF_SIZE);
	// initialize ACC
	ACC_ = (unit_t*)malloc(ACC_SIZE);
	

	for (int i = 0; i < (CACHE_SIZE / (int)sizeof(unit_t)); i++) {
		CACHE_[i] = 0;
	}
	for (int i = 0; i < (SRF_SIZE / (int)sizeof(unit_t)); i++) {
		SRF_[i] = 0;
	}
	for (int i = 0; i < (ACC_SIZE / (int)sizeof(unit_t)); i++) {
		ACC_[i] = 0;
	}
	
	for (int i = 0; i < 8; i++){cache_dirty[i]=false; cache_aam[i] = 0;}

	cache_written = false;
	
	idle_row = IDLE_ROW << (config_.ro_pos + config_.shift_bits); // 528sumin use idle row instead of -1
}


void PimUnit::init(uint8_t* pmemAddr, uint64_t pmemAddr_size, unsigned int burstSize) {
	pmemAddr_ = pmemAddr;
	pmemAddr_size_ = pmemAddr_size;
	burstSize_ = burstSize;
	operand_cache = 0;
	PPC = 0;
	LC = 0;
	cache_written = false;
	for (int i = 0; i < 8; i++){cache_dirty[i]=false;}
}

void PimUnit::SetSrf(uint8_t* DataPtr){
    memcpy(SRF_, DataPtr, SRF_SIZE);
}

bool PimUnit::PIM_OP() {
	// one of cache is used for operands for pim
	// the other is used for banks to R/W
	// change operand cache at every PIM_OP
	operand_cache = !operand_cache ? 1 : 0;

	// PIM_READ has read some and stored in cache
	// if there were no PIM_READ -> Cache is not updated -> cache_written is 0
	// therefore no PIM OP is needed
	if (cache_written) {
		Execute();
		// change cache_written to false
		cache_written = false;
		// Point to next PIM_INSTRUCTION
		PPC += 1;
	}

	// Deal with PIM operation NOP & JUMP
	//  Performed by using LC(Loop Counter)
	//  LC copies the number of iterations and gets lower by 1 when executed
	//  Repeats until LC gets to 1 and escapes the iteration
	if (CRF[PPC].PIM_OP == PIM_OPERATION::JUMP) {
		if (LC == 0) {
			LC = CRF[PPC].imm1_;
			PPC += (uint8_t)CRF[PPC].imm0_;
		}
		else if (LC > 1) {
			PPC += (uint8_t)CRF[PPC].imm0_;
			LC -= 1;
		}
		else if (LC == 1) {
			PPC += 1;
			LC = 0;
		}
	}

	// When pointed PIM_INSTRUCTION is EXIT, ¥ìkernel is finished
	// Reset PPC and return EXIT_END
	if (CRF[PPC].PIM_OP == PIM_OPERATION::EXIT) {
		PPC = 0;
		return true;
	}
	// return false to maintain BG-mode
	return false;
}

void PimUnit::Execute() {
	// currently only support ADD
	switch (CRF[PPC].PIM_OP) {
	case PIM_OPERATION::ADD:
		_ADD();
		break;
	case PIM_OPERATION::MUL:
		_MUL();
		break;
	case PIM_OPERATION::BN:
	        _BN();
	        break;
	case PIM_OPERATION::GEMV:
		_GEMV();
		break;
	case PIM_OPERATION::LD: // load to cache, nothing to calculate
		break;
	case PIM_OPERATION::ST: // store cache with ACC
		_ST();
	        break;
	default:
		std::cout << "not add" << std::endl;
	}
}

unsigned PimUnit::GetSourceBank() {
	int dst = CRF[PPC].dst_;
	unsigned source_bank = 0;
	switch (CRF[PPC].PIM_OP) {
	case PIM_OPERATION::ADD:
		source_bank = (dst & 0b1) ? 0b0101 : 0b1010;
		break;
	default:
		std::cout << "currently only support add" << std::endl;
	}
	return source_bank;
}


void PimUnit::Pim_Read(uint64_t hex_addr, BaseRow base_row){
	uint64_t source_addr = 0;

	// 528sumin now not using GetSourceBank()
	// 528sumin use src in PimInstruction directly
	//unsigned source_bank = GetSourceBank();
	unsigned source_bank = CRF[PPC].src_ & 0xf;
		
	int RW_cache_index = (int)(!operand_cache);
	
	uint64_t ba_offset = 0;

	if (source_bank & 0b1){
		if (base_row.ba0_ == idle_row) {  // 528sumin use idle row
			std::cerr << "ba0 not a valid base row_R" << std::endl;
			exit(1);
		}
		ba_offset = (uint64_t)0 << (config_.ba_pos + config_.shift_bits);
		source_addr = hex_addr + base_row.ba0_ + ba_offset;	
		memcpy((CACHE_+(RW_cache_index*UNITS_PER_WORD)), pmemAddr_ + source_addr, WORD_SIZE);
		cache_written = true;
		cache_aam[RW_cache_index] = (uint8_t)((source_addr >> (config_.co_pos + config_.shift_bits)) & 0x3f);
	}
	if (source_bank & 0b10) {
		if (base_row.ba1_ == idle_row) {
			std::cerr << "ba1 not a valid base rowR" << std::endl;
			exit(1);
		}
		ba_offset = (uint64_t)1 << (config_.ba_pos + config_.shift_bits);
		source_addr = hex_addr + base_row.ba1_ + ba_offset;
		memcpy((CACHE_ + (1 * 2 + RW_cache_index) * UNITS_PER_WORD), pmemAddr_ + source_addr, WORD_SIZE);
		cache_written = true;
		cache_aam[RW_cache_index+2] = (uint8_t)((source_addr >> (config_.co_pos + config_.shift_bits)) & 0x3f);
	}
	if (source_bank & 0b100) {
		if (base_row.ba2_ == idle_row) {
			std::cerr << "ba2 not a valid base row" << std::endl;
			exit(1);
		}
		ba_offset = (uint64_t)2 << (config_.ba_pos + config_.shift_bits);
		source_addr = hex_addr + base_row.ba2_ + ba_offset;
		memcpy((CACHE_ + (2 * 2 + RW_cache_index) * UNITS_PER_WORD), pmemAddr_ + source_addr, WORD_SIZE);
		cache_written = true;
		cache_aam[RW_cache_index+4] = (uint8_t)((source_addr >> (config_.co_pos + config_.shift_bits)) & 0x3f);
	}
	if (source_bank & 0b1000) {
		if (base_row.ba3_ == idle_row) {
		        std::cerr << "ba3 not a valid base row" << std::endl;
			exit(1);
		}
		ba_offset = (uint64_t)3 << (config_.ba_pos + config_.shift_bits);
		source_addr = hex_addr + base_row.ba3_ + ba_offset;
		memcpy((CACHE_ + (3 * 2 + RW_cache_index) * UNITS_PER_WORD), pmemAddr_ + source_addr, WORD_SIZE);
		cache_written = true;
		cache_aam[RW_cache_index+6] = (uint8_t)((source_addr >> (config_.co_pos + config_.shift_bits)) & 0x3f);
	}
	if (CRF[PPC].src_ & 0x30) {
		// cache is not written, but PIM_OP should run for the next cycle
		cache_written = true;
	}

	return;
}

void PimUnit::Pim_Write(uint64_t hex_addr, BaseRow base_row) {
	int RW_cache_index = (int)(!operand_cache);
	uint64_t drain_addr = 0;
	
	uint64_t ba_offset = 0;
	if (cache_dirty[RW_cache_index]) {
		if (base_row.ba0_ == idle_row) {
			std::cerr << "ba0 not a valid base row_W" << std::endl;
			std::cout << "PPC at error: " << (int)PPC << std::endl;
		        std::cout << "LC at error: " << LC << std::endl;
			exit(1);
		}
		ba_offset = (uint64_t)0 << (config_.ba_pos + config_.shift_bits);
		drain_addr = hex_addr + base_row.ba0_ + ba_offset;
		//if(!pim_id){std::cout << "drain addr: " << std::hex << drain_addr << std::endl;}
		//if(!pim_id){std::cout << "in cache: " << *((uint16_t*)(CACHE_ + (RW_cache_index * UNITS_PER_WORD))) << std::endl;}
		memcpy(pmemAddr_ + drain_addr, (CACHE_ + (RW_cache_index * UNITS_PER_WORD)), WORD_SIZE);
		cache_dirty[RW_cache_index] = false;
	}
	else if (cache_dirty[RW_cache_index + 2]) {
		if (base_row.ba1_ == idle_row) {
			std::cerr << "ba1 not a valid base row" << std::endl;
			exit(1);
		}
		ba_offset = (uint64_t)1 << (config_.ba_pos + config_.shift_bits);
		drain_addr = hex_addr + base_row.ba1_ + ba_offset;
		//if(!pim_id){std::cout << "drain addr: " << std::hex << drain_addr << std::endl;}
		//if(!pim_id){std::cout << "base ba1 z: " << std::hex << base_row.ba1_ << std::endl;}
		memcpy(pmemAddr_ + drain_addr, (CACHE_ + (1 * 2 + RW_cache_index) * UNITS_PER_WORD), WORD_SIZE);
		cache_dirty[RW_cache_index + 2] = false;
	}
	else if (cache_dirty[RW_cache_index + 4]) {
		if (base_row.ba2_ == idle_row) {
			std::cerr << "ba2 not a valid base row" << std::endl;
			exit(1);
		}
		ba_offset = (uint64_t)2 << (config_.ba_pos + config_.shift_bits);
		drain_addr = hex_addr + base_row.ba2_ + ba_offset;
		//if(!pim_id){std::cout << "drain addr: " << std::hex << drain_addr << std::endl;}
		memcpy(pmemAddr_ + drain_addr, (CACHE_ + (2 * 2 + RW_cache_index) * UNITS_PER_WORD), WORD_SIZE);
		cache_dirty[RW_cache_index + 4] = false;
	}
	else if (cache_dirty[RW_cache_index + 6]) {
		if (base_row.ba3_ == idle_row) {
			std::cerr << "ba3 not a valid base row_w" << std::endl;
			exit(1);
		}
		ba_offset = (uint64_t)3 << (config_.ba_pos + config_.shift_bits);
		drain_addr = hex_addr + base_row.ba3_ + ba_offset;
		//if(!pim_id){std::cout << "drain addr: " << std::hex << drain_addr << std::endl;}
		memcpy(pmemAddr_ + drain_addr, (CACHE_ + (3 * 2 + RW_cache_index) * UNITS_PER_WORD), WORD_SIZE);
		cache_dirty[RW_cache_index + 6] = false;
	}
}

void PimUnit::_ADD() {

	unit_t* dst;
	unit_t* src0;
	unit_t* src1;

	dst = CACHE_ + (CRF[PPC].dst_ * 2 + operand_cache) * UNITS_PER_WORD;
	cache_dirty[CRF[PPC].dst_ * 2 + operand_cache] = true;

	if (CRF[PPC].dst_ & 1) {
		src0 = CACHE_ + operand_cache * UNITS_PER_WORD;
		src1 = CACHE_ + (2 * 2 + operand_cache) * UNITS_PER_WORD;
	}
	else {
		src0 = CACHE_ + (1 * 2 + operand_cache) * UNITS_PER_WORD;
		src1 = CACHE_ + (3 * 2 + operand_cache) * UNITS_PER_WORD;
	}

	for (int i = 0; i < 16; i++) {
		dst[i] = src0[i] + src1[i];
	}
}

void PimUnit::_MUL() {
	unit_t* dst;
	unit_t* src0;
	unit_t* src1;

	dst = CACHE_ + (CRF[PPC].dst_ * 2 + operand_cache) * UNITS_PER_WORD;
	cache_dirty[CRF[PPC].dst_ * 2 + operand_cache] = true;

	if (CRF[PPC].dst_ & 0b10) {
		src0 = CACHE_ + operand_cache * UNITS_PER_WORD;
		src1 = CACHE_ + (1 * 2 + operand_cache) * UNITS_PER_WORD;
	}
	else {
		src0 = CACHE_ + (2 * 2 + operand_cache) * UNITS_PER_WORD;
		src1 = CACHE_ + (3 * 2 + operand_cache) * UNITS_PER_WORD;
	}

	for (int i = 0; i < 16; i++) {
		dst[i] = src0[i] * src1[i];
	}
}

void PimUnit::_BN() {
	unit_t* dst;
	unit_t* srcx;
	unit_t* srcy;
	unit_t* srcz;
	
	dst = CACHE_ + (CRF[PPC].dst_ * 2 + operand_cache) * UNITS_PER_WORD;
	cache_dirty[CRF[PPC].dst_ * 2 + operand_cache] = true;
	
	if(CRF[PPC].dst_ == 0){
		srcx = CACHE_ + (2 * 2 + operand_cache) * UNITS_PER_WORD;
		srcy = CACHE_ + (3 * 2 + operand_cache) * UNITS_PER_WORD;
		srcz = CACHE_ + (1 * 2 + operand_cache) * UNITS_PER_WORD;
	}
	else if(CRF[PPC].dst_ == 1){
		srcx = CACHE_ + (3 * 2 + operand_cache) * UNITS_PER_WORD;
		srcy = CACHE_ + (2 * 2 + operand_cache) * UNITS_PER_WORD;
		srcz = CACHE_ + (operand_cache) * UNITS_PER_WORD;
	}
	else if(CRF[PPC].dst_ == 2){
		srcx = CACHE_ + (operand_cache) * UNITS_PER_WORD;
		srcy = CACHE_ + (1 * 2 + operand_cache) * UNITS_PER_WORD;
		srcz = CACHE_ + (3 * 2 + operand_cache) * UNITS_PER_WORD;
	}
	else if(CRF[PPC].dst_ == 3){
		srcx = CACHE_ + (1 * 2 + operand_cache) * UNITS_PER_WORD;
		srcy = CACHE_ + (operand_cache) * UNITS_PER_WORD;
		srcz = CACHE_ + (2 * 2 + operand_cache) * UNITS_PER_WORD;
	}
	else{ std::cerr << "not proper dst\n"; exit(1); }
	
	for (int i = 0; i < 16; i++) {
		dst[i] = srcx[i] * srcy[i] + srcz[i];
	}
}

void PimUnit::_GEMV(){
	unit_t* dst;
	unit_t* src0;
	unit_t* src1;
	int vec_index;	
	
	dst = ACC_ + ((CRF[PPC].dst_ - 4) * UNITS_PER_WORD);
	
	if(CRF[PPC].dst_ == 4){ // when bank0 and bank 2 are source
	    src0 = CACHE_ + (operand_cache) * UNITS_PER_WORD;
	    src1 = CACHE_ + (2 * 2 + operand_cache) * UNITS_PER_WORD;    
	    vec_index = (int)(cache_aam[0 + operand_cache] & 0b111);	    
	}
	else if(CRF[PPC].dst_ == 5){ // when bank 1 and bank 3 are source
	    src0 = CACHE_ + (1 * 2 + operand_cache) * UNITS_PER_WORD;
	    src1 = CACHE_ + (3 * 2 + operand_cache) * UNITS_PER_WORD;   
	    vec_index = (int)(cache_aam[2 + operand_cache] & 0b111);
	}
	else{ std::cerr << "gemv dst not properly set\n"; exit(1); }
	
	//std::cout << "SRF? " << SRF_[vec_index*2] << std::endl;
	for (int i = 0; i < 16; i++) {
		dst[i] += src0[i] * SRF_[vec_index*2] + src1[i] * SRF_[vec_index*2+1];
	}
}

void PimUnit::_ST(){
	unit_t* dst;
	unit_t* src;
	
	dst = CACHE_ + (CRF[PPC].dst_ * 2 + operand_cache) * UNITS_PER_WORD;
	cache_dirty[CRF[PPC].dst_ * 2 + operand_cache] = true;
	
	src = ACC_ + UNITS_PER_WORD * (CRF[PPC].dst_ & 1); // src is ACC[0] when dst even, ACC[1] when dst odd
	
	for (int i = 0; i < 16; i++) {
		dst[i] = src[i];
		src[i] = 0;
	}	
}

} // dramsim
