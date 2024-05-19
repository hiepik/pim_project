#ifndef __COMMON_H
#define __COMMON_H

#include <stdint.h>
#include <iostream>
#include <vector>
#include <string>

namespace dramsim3 {

struct Address {
    Address()
        : channel(-1), rank(-1), bankgroup(-1), bank(-1), row(-1), column(-1) {}
    Address(int channel, int rank, int bankgroup, int bank, int row, int column)
        : channel(channel),
          rank(rank),
          bankgroup(bankgroup),
          bank(bank),
          row(row),
          column(column) {}
    Address(const Address& addr)
        : channel(addr.channel),
          rank(addr.rank),
          bankgroup(addr.bankgroup),
          bank(addr.bank),
          row(addr.row),
          column(addr.column) {}
    int channel;
    int rank;
    int bankgroup;
    int bank;
    int row;
    int column;
};

// CAPSTONE base row structure
struct BaseRow {
    BaseRow()
        : ba0_(-1), ba1_(-1), ba2_(-1), ba3_(-1) {}
    BaseRow(int ba0, int ba1, int ba2, int ba3)
        : ba0_(ba0), ba1_(ba1), ba2_(ba2), ba3_(ba3) {}
    uint64_t ba0_;
    uint64_t ba1_;
    uint64_t ba2_;
    uint64_t ba3_;
};

inline uint32_t ModuloWidth(uint64_t addr, uint32_t bit_width, uint32_t pos) {
    addr >>= pos;
    auto store = addr;
    addr >>= bit_width;
    addr <<= bit_width;
    return static_cast<uint32_t>(store ^ addr);
}

// extern std::function<Address(uint64_t)> AddressMapping;
int GetBitInPos(uint64_t bits, int pos);
// it's 2017 and c++ std::string still lacks a split function, oh well
std::vector<std::string> StringSplit(const std::string& s, char delim);
template <typename Out>
void StringSplit(const std::string& s, char delim, Out result);

int LogBase2(int power_of_two);
void AbruptExit(const std::string& file, int line);
bool DirExist(std::string dir);

enum class CommandType {
    READ,
    READ_PRECHARGE,
    WRITE,
    WRITE_PRECHARGE,
    ACTIVATE,
    PRECHARGE,
    REFRESH_BANK,
    REFRESH,
    SREF_ENTER,
    SREF_EXIT,
    SIZE
};

struct Command {
    Command() : cmd_type(CommandType::SIZE), hex_addr(0) {}
    Command(CommandType cmd_type, const Address& addr, uint64_t hex_addr)
        : cmd_type(cmd_type), addr(addr), hex_addr(hex_addr) {}
    Command(CommandType cmd_type, const Address& addr, uint64_t hex_addr, std::string executed_bankmode)    // >> mmm <<
        : cmd_type(cmd_type), addr(addr), hex_addr(hex_addr), executed_bankmode(executed_bankmode) {}
    // Command(const Command& cmd) {}

    bool IsValid() const { return cmd_type != CommandType::SIZE; }
    bool IsRefresh() const {
        return cmd_type == CommandType::REFRESH ||
               cmd_type == CommandType::REFRESH_BANK;
    }
    bool IsRead() const {
        return cmd_type == CommandType::READ ||
               cmd_type == CommandType ::READ_PRECHARGE;
    }
    bool IsWrite() const {
        return cmd_type == CommandType ::WRITE ||
               cmd_type == CommandType ::WRITE_PRECHARGE;
    }
    bool IsReadWrite() const { return IsRead() || IsWrite(); }
    bool IsRankCMD() const {
        return cmd_type == CommandType::REFRESH ||
               cmd_type == CommandType::SREF_ENTER ||
               cmd_type == CommandType::SREF_EXIT;
    }
    CommandType cmd_type;
    Address addr;
    uint64_t hex_addr;
    std::string executed_bankmode;  // >> mmm <<

    int Channel() const { return addr.channel; }
    int Rank() const { return addr.rank; }
    int Bankgroup() const { return addr.bankgroup; }
    int Bank() const { return addr.bank; }
    int Row() const { return addr.row; }
    int Column() const { return addr.column; }

    friend std::ostream& operator<<(std::ostream& os, const Command& cmd);
};

struct Transaction {
    Transaction() {}
    Transaction(uint64_t addr, bool is_write, uint8_t* DataPtr)
        : addr(addr),
          added_cycle(0),
          complete_cycle(0),
          DataPtr(DataPtr),
          is_write(is_write) {}
    Transaction(const Transaction& tran)
        : addr(tran.addr),
          added_cycle(tran.added_cycle),
          complete_cycle(tran.complete_cycle),
          DataPtr(tran.DataPtr),
          is_write(tran.is_write),
          executed_bankmode(tran.executed_bankmode) {}
    uint64_t addr;
    uint64_t added_cycle;
    uint64_t complete_cycle;
    uint8_t* DataPtr;               // holds the data needed for physical memory
                                    // RD/WR
                                    // e.g., READ transaction : DataPtr holds
                                    // read data from physical memory
                                    // e.g., WRITE transaction : DataPtr holds
                                    // the data to write on physical memory
    bool is_write;
    std::string executed_bankmode;  // expresses transaction's executed bank
                                    // mode

    friend std::ostream& operator<<(std::ostream& os, const Transaction& trans);
    friend std::istream& operator>>(std::istream& is, Transaction& trans);
};

}  // namespace dramsim3
#endif
