#ifndef __PIMUNIT_H_
#define __PIMUNIT_H_

#include <sys/mman.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <cstring>
#include <cmath>
#include "./pim_config.h"
// #include "./pim_utils.h"
#include "./configuration.h"
#include "./common.h"


namespace dramsim3 {

class PimUnit {
public:
	PimUnit(Config& config, int id);
	void init(uint8_t* pmemAddr, uint64_t pmemAddr_size, unsigned int burstSize);

	uint8_t PPC; // program counter
	int LC;
	int pim_id;

	bool cache_written = false;
	bool cache_dirty[8];

	PimInstruction CRF[32];

	void Pim_Read(uint64_t hex_addr, BaseRow base_row);
	void PIM_OP();
	void Pim_Write(uint64_t hex_addr, BaseRow base_row);
	void Execute();

	unsigned GetSourceBank();

	void _ADD();

	unit_t* CACHE_;

	uint8_t* pmemAddr_;
	uint64_t pmemAddr_size_;
	unsigned int burstSize_;

protected:
	Config& config_;

private:
	unsigned operand_cache;
};

} // dramsim


#endif  // PIMUNIT_H_
















