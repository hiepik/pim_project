#include "./pim_func_sim.h"


namespace dramsim3 {
PimFuncSim::PimFuncSim(Config& config)
    : config_(config) {
    // Set pim_unit's id by its order of pim_unit (= pim_index)
    for (int i = 0; i < config_.channels * config_.banks / 4; i++) {
        pim_unit_.push_back(new PimUnit(config_, i));
    }
    base_row_ = BaseRow();
}

void PimFuncSim::init(uint8_t* pmemAddr_, uint64_t pmemAddr_size_,
    unsigned int burstSize_) {
    burstSize = burstSize_;
    pmemAddr_size = pmemAddr_size_;
    pmemAddr = pmemAddr_;


    // Set default bankmode of channel to "SB"
    for (int i = 0; i < config_.channels; i++) {
        bankmode.push_back("SB");
    }
    
    std::cout << "PimFuncSim initialized!\n";

    for (int i = 0; i < config_.channels * config_.banks / 4; i++) {
        pim_unit_[i]->init(pmemAddr, pmemAddr_size, burstSize);
    }
    std::cout << "pim_units initialized!\n";
}

// Map structured address into 64-bit hex_address
uint64_t PimFuncSim::ReverseAddressMapping(Address& addr) {
    uint64_t hex_addr = 0;
    hex_addr += (uint64_t)addr.channel << config_.ch_pos;
    hex_addr += (uint64_t)addr.rank << config_.ra_pos;
    hex_addr += (uint64_t)addr.bankgroup << config_.bg_pos;
    hex_addr += (uint64_t)addr.bank << config_.ba_pos;
    hex_addr += (uint64_t)addr.row << config_.ro_pos;
    hex_addr += (uint64_t)addr.column << config_.co_pos;
    return hex_addr << config_.shift_bits;
}

/**************************************************
* 
* 
* Should make MODCHANGER
* What mode do we need ?????????
* 
* 
*****************************************************/
bool PimFuncSim::ModeChanger(uint64_t hex_addr) {
    Address addr = config_.AddressMapping(hex_addr);
    if (addr.row == SB_ROW) {
        if (bankmode[addr.channel] == "BG") {
            bankmode[addr.channel] = "SB";
        }
        //if (DebugMode(hex_addr))
        //    std::cout << " Pim_func_sim: BG ¡æ SB mode change\n";
        return true;
    }
    else if (addr.row == BG_ROW) {
        if (bankmode[addr.channel] == "SB") {
            bankmode[addr.channel] = "BG";
        }
        //if (DebugMode(hex_addr))
        //    std::cout << " Pim_func_sim: SB ¡æ BG mode change\n";
        return true;
    }
    return false;
}

// Write DataPtr data to physical memory address of hex_addr
void PimFuncSim::PmemWrite(uint64_t hex_addr, uint8_t* DataPtr) {
    uint8_t* host_addr = pmemAddr + hex_addr;
    //std::cout << "is write working?: " << *((uint16_t*)DataPtr) << std::endl;
    memcpy(host_addr, DataPtr, burstSize);
}

// Return pim_index of pim_unit that input address accesses
uint64_t PimFuncSim::GetPimIndex(Address& addr) {
    return (addr.channel * config_.bankgroups + addr.bankgroup);
}

// Read data from physical memory address of hex_addr to DataPtr
void PimFuncSim::PmemRead(uint64_t hex_addr, uint8_t* DataPtr) {
    uint8_t* host_addr = pmemAddr + hex_addr;
    memcpy(DataPtr, host_addr, burstSize);
}

void PimFuncSim::DRAM_IO(Transaction* trans) {
    uint64_t hex_addr = (*trans).addr;
    uint8_t* DataPtr = (*trans).DataPtr;
    Address addr = config_.AddressMapping(hex_addr);
    bool is_write = (*trans).is_write;

    // Change bankmode register if transaction has certain row address
    bool is_mode_change = ModeChanger(hex_addr);
    if (is_mode_change)
        return;

    if (bankmode[addr.channel] == "BG") {
        std::cerr << "DRAM_IO executed in BG mode" << std::endl;
        exit(1);
    }

    // R/W put it in else loop
    //if (DebugMode(hex_addr))
    //    std::cout << "RD/WR\n";
    if (is_write) {
        PmemWrite(hex_addr, DataPtr);
    }
    else {
        PmemRead(hex_addr, DataPtr);
    }
    
    return;
}


// run PIM_OP on all bankgroups on a channel
void PimFuncSim::PIM_OP(int channel) {
    for (int i = 0; i < 4; i++) {
        pim_unit_[channel * config_.bankgroups + i]->PIM_OP();
    }
}

void PimFuncSim::SetBaseRow(BaseRow base_row) {
    base_row_ = base_row;
}

void PimFuncSim::PushCRF(PimInstruction* kernel) {
    PimInstruction* p = kernel;
    for (int i = 0; i < config_.channels * config_.banks / 4; i++) {
        p = kernel;
        //std::cout << "address of p is: " << std::hex << p << std::endl;
        //std::cout << "OP of p is: " << (int)((*p).PIM_OP) << std::endl;
        for (int j = 0; j < 32; j++) {
            pim_unit_[i]->CRF[j] = *p;
            p++;
        }
    }
}

// 528sumin --> changed the way pim_func_sim encode the command address and broadcast to all pim_units
void PimFuncSim::PIM_Read(Command cmd) {
    int channel_ = cmd.Channel();
    // cmd is the one for bank0
    // should change row bank0 to row_offset and send it to pim_units
    //uint64_t base_addr = cmd.hex_addr;
    Address addr = config_.AddressMapping(cmd.hex_addr);
    addr.bank = 0;	// reset bank because pim_unit will broadcast cmd to banks in bankgroup
    for (int i = 0; i < 4; i++) {
        addr.bankgroup = i;
        uint64_t base_addr = ReverseAddressMapping(addr);
        pim_unit_[channel_ * config_.bankgroups + i]->Pim_Read(base_addr, base_row_);
    }
}

// 528sumin --> changed the way pim_func_sim encode the command address and broadcast to all pim_units
void PimFuncSim::PIM_Write(Command cmd) {
    int channel_ = cmd.Channel();
    Address addr = config_.AddressMapping(cmd.hex_addr);
    addr.bank = 0;    // reset bank because pim_unit will broadcast cmd to banks in bankgroup
    for (int i = 0; i < 4; i++) {
        addr.bankgroup = i;
        uint64_t base_addr = ReverseAddressMapping(addr);
        pim_unit_[channel_ * config_.bankgroups + i]->Pim_Write(base_addr, base_row_);
    }
}
}


