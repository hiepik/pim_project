#include "memory_system.h"

namespace dramsim3 {
MemorySystem::MemorySystem(const std::string &config_file,
                           const std::string &output_dir,
                           std::function<void(uint64_t, uint8_t*)> read_callback,
                           std::function<void(uint64_t)> write_callback)
    : config_(new Config(config_file, output_dir)) {
    // TODO(a): ideal memory type?

    dram_system_ = new JedecDRAMSystem(*config_, output_dir, read_callback,
        write_callback);
}

MemorySystem::~MemorySystem() {
    delete (dram_system_);
    delete (config_);
}

void MemorySystem::ClockTick() { dram_system_->ClockTick(); }

double MemorySystem::GetTCK() const { return config_->tCK; }

int MemorySystem::GetBusBits() const { return config_->bus_width; }

int MemorySystem::GetBurstLength() const { return config_->BL; }

int MemorySystem::GetQueueSize() const { return config_->trans_queue_size; }

// void MemorySystem::RegisterCallbacks(
//    std::function<void(uint64_t, uint8_t*)> read_callback,
//    std::function<void(uint64_t)> write_callback) {
//    dram_system_->RegisterCallbacks(read_callback, write_callback);
// }

bool MemorySystem::WillAcceptTransaction(uint64_t hex_addr,
                                         bool is_write) const {
    return dram_system_->WillAcceptTransaction(hex_addr, is_write);
}

bool MemorySystem::AddTransaction(uint64_t hex_addr, bool is_write, uint8_t *DataPtr) {
    return dram_system_->AddTransaction(hex_addr, is_write, DataPtr);
}

void MemorySystem::PrintStats() const { dram_system_->PrintStats(); }

void MemorySystem::ResetStats() { dram_system_->ResetStats(); }

MemorySystem* GetMemorySystem(const std::string &config_file, const std::string &output_dir,
                 std::function<void(uint64_t, uint8_t*)> read_callback,
                 std::function<void(uint64_t)> write_callback) {
    return new MemorySystem(config_file, output_dir, read_callback, write_callback);
}

void MemorySystem::init(uint8_t* pmemAddr, uint64_t size, unsigned int burstSize) {
	dram_system_->init(pmemAddr, size, burstSize);
    // dram_system_->pmemAddr = pmemAddr;
    // dram_system_->pmemAddr_size = size;
    // dram_system_->burstSize = burstSize;
}

bool MemorySystem::IsPendingTransaction() {
    return dram_system_->IsPendingTransaction();
}

void MemorySystem::SetMode(int mode) { dram_system_->SetMode(mode); }

void MemorySystem::PushCRF(PimInstruction* kernel) { dram_system_->PushCRF(kernel); }


void MemorySystem::SetWriteBufferThreshold(int threshold) {
    dram_system_->SetWriteBufferThreshold(threshold);
}

void MemorySystem::SetBaseRow(BaseRow baserow) {
    dram_system_->SetBaseRow(baserow);
}

}  // namespace dramsim3

// This function can be used by autoconf AC_CHECK_LIB since
// apparently it can't detect C++ functions.
// Basically just an entry in the symbol table
extern "C" {
void libdramsim3_is_present(void) { ; }
}
