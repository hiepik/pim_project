#ifndef __MEMORY_SYSTEM__H
#define __MEMORY_SYSTEM__H

#include <functional>
#include <string>

#include "configuration.h"
#include "dram_system.h"
#include "./pim_config.h"


namespace dramsim3 {

// This should be the interface class that deals with CPU
class MemorySystem {
 public:
    MemorySystem(const std::string &config_file, const std::string &output_dir,
                 std::function<void(uint64_t, uint8_t*)> read_callback,
                 std::function<void(uint64_t)> write_callback);
    ~MemorySystem();
    void ClockTick();
    // void RegisterCallbacks(std::function<void(uint64_t, uint8_t*)> read_callback,
    //                        std::function<void(uint64_t)> write_callback);
    double GetTCK() const;
    int GetBusBits() const;
    int GetBurstLength() const;
    int GetQueueSize() const;
    void PrintStats() const;
    void ResetStats();

    bool WillAcceptTransaction(uint64_t hex_addr, bool is_write) const;
    bool AddTransaction(uint64_t hex_addr, bool is_write, uint8_t *DataPtr);
    void init(uint8_t* pmemAddr, uint64_t size, unsigned int burstSize);

    // For barrier
    bool IsPendingTransaction();
    void SetWriteBufferThreshold(int threshold);

    void SetBaseRow(BaseRow baserow);
    void SetMode(int mode);
    void PushCRF(PimInstruction* kernel);

 private:
    // These have to be pointers because Gem5 will try to push this object
    // into container which will invoke a copy constructor, using pointers
    // here is safe
    Config *config_;
    BaseDRAMSystem *dram_system_;
};

MemorySystem* GetMemorySystem(const std::string &config_file, const std::string &output_dir,
                 std::function<void(uint64_t, uint8_t*)> read_callback,
                 std::function<void(uint64_t)> write_callback);

}  // namespace dramsim3

#endif
