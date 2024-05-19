#ifndef __DRAM_SYSTEM_H
#define __DRAM_SYSTEM_H

#include <iostream>
#include <fstream>
#include <string>
#include <vector>

#include "./common.h"
#include "./configuration.h"
#include "./controller.h"
#include "./timing.h"
#include "./pim_func_sim.h"
#include "./pim_config.h"

#ifdef THERMAL
#include "./thermal.h"
#endif  // THERMAL

namespace dramsim3 {

class BaseDRAMSystem {
 public:
    BaseDRAMSystem(Config &config, const std::string &output_dir,
                   std::function<void(uint64_t, uint8_t*)> read_callback,
                   std::function<void(uint64_t)> write_callback);
    virtual ~BaseDRAMSystem() {}
    // void RegisterCallbacks(std::function<void(uint64_t, uint8_t*)> read_callback,
    //                        std::function<void(uint64_t)> write_callback);
    void PrintEpochStats();
    void PrintStats();
    void ResetStats();

    virtual bool WillAcceptTransaction(uint64_t hex_addr,
                                       bool is_write) const = 0;
    virtual bool AddTransaction(uint64_t hex_addr, bool is_write,
                                uint8_t *DataPtr) = 0;
    virtual void ClockTick() = 0;
    int GetChannel(uint64_t hex_addr) const;

    // For barrier
    bool IsPendingTransaction();
    void SetWriteBufferThreshold(int threshold);

    void SetMode(int mode); // CAPSTONE
    int mode_;

    std::function<void(uint64_t req_id, uint8_t* DataPtr)> read_callback_;
    std::function<void(uint64_t req_id)> write_callback_;
    static int total_channels_;

    uint8_t* pmemAddr;
    uint64_t pmemAddr_size;
    unsigned int burstSize;
    PimFuncSim *pim_func_sim_;
    void init(uint8_t* pmemAddr, uint64_t pmemAddr_size,
              unsigned int burstSize);
    
    void SetBaseRow(BaseRow baserow);
    void PushCRF(PimInstruction* kernel);

 protected:
    uint64_t id_;
    uint64_t last_req_clk_;
    Config &config_;
    Timing timing_;
    uint64_t parallel_cycles_;
    uint64_t serial_cycles_;

#ifdef THERMAL
    ThermalCalculator thermal_calc_;
#endif  // THERMAL

    uint64_t clk_;
    std::vector<Controller*> ctrls_;

#ifdef ADDR_TRACE
    std::ofstream address_trace_;
#endif  // ADDR_TRACE
};

// hmmm not sure this is the best naming...
class JedecDRAMSystem : public BaseDRAMSystem {
 public:
    JedecDRAMSystem(Config &config, const std::string &output_dir,
                    std::function<void(uint64_t, uint8_t*)> read_callback,
                    std::function<void(uint64_t)> write_callback);
    ~JedecDRAMSystem();
    bool WillAcceptTransaction(uint64_t hex_addr, bool is_write) const override;
    bool AddTransaction(uint64_t hex_addr, bool is_write,
                        uint8_t *DataPtr) override;
    void ClockTick() override;
};

}  // namespace dramsim3
#endif  // __DRAM_SYSTEM_H
