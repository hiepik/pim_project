#include "./pim_unit.h"

namespace dramsim3 {


PimUnit::PimUnit(Config& config, int id) : pim_id(id), config_(config) {
	PPC = 0;
	LC = 0;
	operand_cache = 0;

	// initialize Cache's
	CACHE_ = (unit_t*)malloc(CACHE_SIZE);

	for (int i = 0; i < (CACHE_SIZE / (int)sizeof(unit_t)); i++) {
		CACHE_[i] = 0;
	}

	cache_written = false;
}


void PimUnit::init(uint8_t* pmemAddr, uint64_t pmemAddr_size, unsigned int burstSize) {
	pmemAddr_ = pmemAddr;
	pmemAddr_size_ = pmemAddr_size;
	burstSize_ = burstSize;
	operand_cache = 0;
	PPC = 0;
	LC = 0;
	cache_written = false;
}

void PimUnit::PIM_OP() {
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
		return;
	}
	return;
}

void PimUnit::Execute() {
	// currently only support ADD
	switch (CRF[PPC].PIM_OP) {
	case PIM_OPERATION::ADD:
		_ADD();
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

	unsigned source_bank = GetSourceBank();

	int RW_cache_index = (int)(!operand_cache);

	if (source_bank & 0b1){
		if (base_row.ba0_ == -1) {
			std::cerr << "ba0 not a valid base row_R" << std::endl;
			exit(1);
		}
		source_addr = hex_addr + base_row.ba0_;
		memcpy((CACHE_+(RW_cache_index*UNITS_PER_WORD)), pmemAddr_ + hex_addr, WORD_SIZE);
		cache_written = true;
	}
	if (source_bank & 0b10) {
		if (base_row.ba1_ == -1) {
			std::cerr << "ba1 not a valid base row" << std::endl;
			exit(1);
		}
		source_addr = hex_addr + base_row.ba1_;
		memcpy((CACHE_ + (1 * 2 + RW_cache_index) * UNITS_PER_WORD), pmemAddr_ + hex_addr, WORD_SIZE);
		cache_written = true;
	}
	if (source_bank & 0b100) {
		if (base_row.ba2_ == -1) {
			std::cerr << "ba2 not a valid base row" << std::endl;
			exit(1);
		}
		source_addr = hex_addr + base_row.ba2_;
		memcpy((CACHE_ + (2 * 2 + RW_cache_index) * UNITS_PER_WORD), pmemAddr_ + hex_addr, WORD_SIZE);
		cache_written = true;
	}
	if (source_bank & 0b1000) {
		if (base_row.ba3_ == -1) {
		        
			exit(1);
		}
		source_addr = hex_addr + base_row.ba3_;
		memcpy((CACHE_ + (3 * 2 + RW_cache_index) * UNITS_PER_WORD), pmemAddr_ + hex_addr, WORD_SIZE);
		cache_written = true;
	}

	return;
}

void PimUnit::Pim_Write(uint64_t hex_addr, BaseRow base_row) {
	int RW_cache_index = (int)(!operand_cache);
	uint64_t drain_addr = 0;

	if (CRF[PPC].dst_ == 0) {
		if (base_row.ba0_ == -1) {
			std::cerr << "ba0 not a valid base row_W" << std::endl;
			std::cout << "PPC at error: " << (int)PPC << std::endl;
		        std::cout << "LC at error: " << LC << std::endl;
			exit(1);
		}
		drain_addr = hex_addr + base_row.ba0_;
		memcpy(pmemAddr_ + hex_addr, (CACHE_ + (RW_cache_index * UNITS_PER_WORD)), WORD_SIZE);
	}
	else if (CRF[PPC].dst_ == 1) {
		if (base_row.ba1_ == -1) {
			std::cerr << "ba1 not a valid base row" << std::endl;
			exit(1);
		}
		drain_addr = hex_addr + base_row.ba1_;
		memcpy(pmemAddr_ + hex_addr, (CACHE_ + (1 * 2 + RW_cache_index) * UNITS_PER_WORD), WORD_SIZE);
	}
	else if (CRF[PPC].dst_ == 2) {
		if (base_row.ba2_ == -1) {
			std::cerr << "ba2 not a valid base row" << std::endl;
			exit(1);
		}
		drain_addr = hex_addr + base_row.ba2_;
		memcpy(pmemAddr_ + hex_addr, (CACHE_ + (2 * 2 + RW_cache_index) * UNITS_PER_WORD), WORD_SIZE);
	}
	else {
		if (base_row.ba3_ == -1) {
			std::cerr << "ba3 not a valid base row_w" << std::endl;
			exit(1);
		}
		drain_addr = hex_addr + base_row.ba3_;
		memcpy(pmemAddr_ + hex_addr, (CACHE_ + (3 * 2 + RW_cache_index) * UNITS_PER_WORD), WORD_SIZE);
	}
}

void PimUnit::_ADD() {

	unit_t* dst;
	unit_t* src0;
	unit_t* src1;

	dst = CACHE_ + (CRF[PPC].dst_ * 2 + operand_cache) * UNITS_PER_WORD;

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

} // dramsim
