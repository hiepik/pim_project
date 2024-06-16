#ifndef __PIM_FUNC_SIM_H
#define __PIM_FUNC_SIM_H


#include <iostream>
#include <fstream>
#include "configuration.h"
#include "common.h"
#include "pim_unit.h"
#include "pim_config.h"

#define SB_ROW             0x3fff
#define BG_ROW             0x3ffe
#define ABG_ROW	    0x3ffd

namespace dramsim3 {


class PimFuncSim {
public:
	PimFuncSim(Config& config);
	void DRAM_IO(Transaction* trans);
	bool ModeChanger(uint64_t hex_addr);
	void PIM_Read(Command cmd);
	void PIM_Write(Command cmd);
	void PIM_OP(int channel);

	std::vector<string> bankmode;
	std::vector<PimUnit*> pim_unit_;

	void PushCRF(PimInstruction* kernel);
	
	void SetBaseRow(BaseRow base_row);

	BaseRow base_row_;

	uint8_t* pmemAddr;
	uint64_t pmemAddr_size;
	unsigned int burstSize;
	
	uint64_t GetPimIndex(Address& addr);

	void PmemWrite(uint64_t hex_addr, uint8_t* DataPtr);
	void PmemRead(uint64_t hex_addr, uint8_t* DataPtr);

	uint64_t ReverseAddressMapping(Address& addr);
	void init(uint8_t* pmemAddr, uint64_t pmemAddr_size, unsigned int burstSize);


protected:
	Config& config_;

};


} // namespace dramsim3

#endif // __PIM_FUNC_SIM_H
