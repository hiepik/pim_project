#include "transaction_generator.h"

namespace dramsim3 {

    void TransactionGenerator::ReadCallBack(uint64_t addr, uint8_t* DataPtr) {
        return;
    }
    void TransactionGenerator::WriteCallBack(uint64_t addr) {
        return;
    }

    // Map 64-bit hex_address into structured address
    uint64_t TransactionGenerator::ReverseAddressMapping(Address& addr) {
        uint64_t hex_addr = 0;
        hex_addr += ((uint64_t)addr.channel) << config_->ch_pos;
        hex_addr += ((uint64_t)addr.rank) << config_->ra_pos;
        hex_addr += ((uint64_t)addr.bankgroup) << config_->bg_pos;
        hex_addr += ((uint64_t)addr.bank) << config_->ba_pos;
        hex_addr += ((uint64_t)addr.row) << config_->ro_pos;
        hex_addr += ((uint64_t)addr.column) << config_->co_pos;
        return hex_addr << config_->shift_bits;
    }

    // Returns the minimum multiple of stride that is higher than num
    uint64_t TransactionGenerator::Ceiling(uint64_t num, uint64_t stride) {
        return ((num + stride - 1) / stride) * stride;
    }

    // Send transaction to memory_system (DRAMsim3 + PIM Functional Simulator)
    //  hex_addr : address to RD/WR from physical memory or change bank mode
    //  is_write : denotes to Read or Write
    //  *DataPtr : buffer used for both RD/WR transaction (read common.h)
    void TransactionGenerator::TryAddTransaction(uint64_t hex_addr, bool is_write,
        uint8_t* DataPtr) {
        // Wait until memory_system is ready to get Transaction
        while (!memory_system_.WillAcceptTransaction(hex_addr, is_write)) {
            memory_system_.ClockTick();
            clk_++;
        }
        // Send transaction to memory_system
        if (is_write) {
            uint8_t* new_data = (uint8_t*)malloc(burstSize_);
            std::memcpy(new_data, DataPtr, burstSize_);
            //std::cout << std::hex << clk_ << "\twrite\t" << hex_addr << std::dec << std::endl;
            memory_system_.AddTransaction(hex_addr, is_write, new_data);
            memory_system_.ClockTick();
            clk_++;
        }
        else {
            //std::cout << std::hex << clk_ << "\tread\t" << hex_addr << std::dec << std::endl;
            memory_system_.AddTransaction(hex_addr, is_write, DataPtr);
            memory_system_.ClockTick();
            clk_++;
        }

#if 0
        if (is_write)
            std::cout << std::hex << cnt_ << "\twrite\t" << hex_addr << std::dec << std::endl;
        else
            std::cout << std::hex << cnt_ << "\tread\t" << hex_addr << std::dec << std::endl;
        cnt_++;
#endif

#if 0
        if (is_print_) {
            Address addr = config_->AddressMapping(hex_addr);
            if (addr.channel == 0 && (addr.bank == 0 || addr.bank == 1))
                std::cout << clk_ - start_clk_ << "\t" << std::hex << hex_addr + 0x5000 << std::dec << std::endl;
        }
#endif

    }

    // Prevent turning out of order between transaction parts
    //  Change memory's threshold and wait until all pending transactions are
    //  executed
    void TransactionGenerator::Barrier() {
        //return;
        memory_system_.SetWriteBufferThreshold(0);
        while (memory_system_.IsPendingTransaction()) {
            memory_system_.ClockTick();
            clk_++;
        }
        memory_system_.SetWriteBufferThreshold(-1);
    }



    // Initialize variables and ukernel
    void AddTransactionGenerator::Initialize() {
        // base address of operands
        addr_x_ = 0;
        addr_y_ = Ceiling(n_ * UNIT_SIZE, SIZE_ROW * NUM_BANK);
        addr_z_ = addr_y_ + Ceiling(n_ * UNIT_SIZE, SIZE_ROW * NUM_BANK);

        base_row_x_ = addr_x_;
        base_row_y_ = addr_y_;
        base_row_z_ = addr_z_;
	//std::cout << "base ba0 z real: " << std::hex << base_row_z_ << std::endl;
        row_count_ = Ceiling(n_ * UNIT_SIZE, SIZE_ROW * NUM_BANK) / (SIZE_ROW * NUM_BANK); // 총 몇개의 row 계산이 필요한가?
        op_count_ = Ceiling(n_ * UNIT_SIZE, SIZE_WORD * NUM_BANK) / (SIZE_WORD*NUM_BANK);   // bank당 총 몇 번의 계산이 있을 것인가? 

        ukernel_add_ = (PimInstruction*)malloc(sizeof(PimInstruction) * 32);
        

        // PimInstruction undone
        // for right now (type, dst, imm0, imm1)
        ukernel_add_[0] = PimInstruction(PIM_OPERATION::ADD, 1);
        ukernel_add_[1] = PimInstruction(PIM_OPERATION::JUMP, 0, -1, 7);
        ukernel_add_[2] = PimInstruction(PIM_OPERATION::ADD, 0);
        ukernel_add_[3] = PimInstruction(PIM_OPERATION::JUMP, 0, -1, 7);
        ukernel_add_[4] = PimInstruction(PIM_OPERATION::ADD, 3);
        ukernel_add_[5] = PimInstruction(PIM_OPERATION::JUMP, 0, -1, 7);
        ukernel_add_[6] = PimInstruction(PIM_OPERATION::ADD, 2);
        ukernel_add_[7] = PimInstruction(PIM_OPERATION::JUMP, 0, -1, 7);
        ukernel_add_[8] = PimInstruction(PIM_OPERATION::EXIT, 0);
        
    }


    // Write operand data and μkernel to physical memory and PIM registers
    void AddTransactionGenerator::SetData() {
        // strided size of one operand with one computation part(minimum)
        uint64_t strided_size = Ceiling(n_ * UNIT_SIZE, SIZE_WORD * NUM_BANK);

#ifdef debug_mode
        std::cout << "HOST:\tSet input data\n";
#endif

        uint64_t address;

        // Write input data x to physical memory
        for (int offset = 0; offset < strided_size; offset += SIZE_WORD) {
            address = ADDR_CONV0(addr_x_ + offset);
            TryAddTransaction(address, true, x_ + offset);
        }
        

        // Write input data y to physical memory
        for (int offset = 0; offset < strided_size; offset += SIZE_WORD) {
            address = ADDR_CONV2(addr_y_ + offset);
            TryAddTransaction(address, true, y_ + offset);
        }
        Barrier();

        // 일단은 이 방식으로 모든 PIM UNIT에 CRF를 전송
        memory_system_.PushCRF(ukernel_add_); 
/*

        SETCRF() 기능 나중에 구현

#ifdef debug_mode
        std::cout << "\nHOST:\tProgram μkernel \n";
#endif
        for (int ch = 0; ch < NUM_CHANNEL; ch++) {
            for (int co = 0; co < 4; co++) {
                Address addr(ch, 0, 0, 0, MAP_CRF, co);
                uint64_t hex_addr = ReverseAddressMapping(addr);
                TryAddTransaction(hex_addr, true, (uint8_t*)&ukernel_add_[co * 8]);
            }
        }
        Barrier();
*/
    }


    // Execute PIM computation
    void AddTransactionGenerator::Execute() {

        // mode transition
        /**************************************************
        * mode transition part -> to PIM-BG-OP MODE
        * *************************************************/

        *data_temp_ |= 1;
#ifdef debug_mode
        std::cout << "\nHOST:\t[1] SB -> BG \n";
#endif
        for (int ch = 0; ch < NUM_CHANNEL; ch++) {
            Address addr(ch, 0, 0, 0, MAP_BGMR, 0);
            uint64_t hex_addr = ReverseAddressMapping(addr);
            TryAddTransaction(hex_addr, false, data_temp_);
        }
        Barrier();

        // 모든 controller에 모드 변환을 알림
        // 아직 따로 모드 변환을 알리지 않으면 오류가 나는 상황
        memory_system_.SetMode(1); // tell memory controller to change the controllers mode to BG mode

        // write trans가 2개 이상일 때 부터 write transaction이 cmd로 변환된다.
        memory_system_.SetWriteBufferThreshold(1);

        // X가 4개의 bank 중하나에 있겠지 --> 총 4번 진행
        for (int ba = 0; ba < 4; ba++) {
            // 각 bank마다 base row가 다르기 때문에 이 것을 기록해놓아야함
            // ex) bank 0: X, bank1: Z bank2: Y bank3: NaN
            // X+Y=Z, therefore bank0+bank2=bank1
            BaseRow base_row_;
            switch (ba) {
            case 0:
                base_row_ = BaseRow(base_row_x_, base_row_z_, base_row_y_, -1);
                break;
            case 1:
                base_row_ = BaseRow(base_row_z_, base_row_x_, -1, base_row_y_);
                break;
            case 2:
                base_row_ = BaseRow(base_row_y_, -1, base_row_x_, base_row_z_);
                break;
            case 3:
                base_row_ = BaseRow(-1, base_row_y_, base_row_z_, base_row_x_);
            }
            memory_system_.SetBaseRow(base_row_);  // 메모리 시스템에 base row 보낸다.

            uint64_t bank0_base_row = (uint64_t)base_row_.ba0_; // 첫번째 base row
            // dramsim에서의 cycle 은 전부 각 channel의 첫번째 bank에서 이루어진다.
            // 따라서 BG 모드에서는 모든 transaction을 bank 0로 보낸다.

            /*************************************************
            * Send Transaction
            **************************************************/
            // now which variable bank represent is defined
            // should calculate every row in this defined state
            for (int row_offset = 0; row_offset < row_count_; row_offset++) {
                * data_temp_ |= 1;
                
                for (int col = 0; col < NUM_WORD_PER_ROW; col++) {
                    
                    // all X finished, then break
                    if (row_offset * NUM_WORD_PER_ROW + col == op_count_)
                        break;

                    // send Source Bank Read
                    for (int ch = 0; ch < NUM_CHANNEL; ch++) {
                        Address addr(ch, 0, 0,  0, row_offset, col);
                        uint64_t hex_addr = ReverseAddressMapping(addr);
                        TryAddTransaction(bank0_base_row + hex_addr, false, data_temp_); // row를 더해주는 이유는 1번 bank에만 transaction을 보낼 것이기 때문이다.
                    }
                }

                // write transaction to drain out the results 
                // 2*CCD_L의 딜레이를 계산하기 위해 추가된다.
                // 해당하는 row의 계산이 다 끝나면 2번의 사이클 동안 기다려야 모든 값이 bank에 저장된다.
                // 이 시간을 DRAMSIM에 구현하기 위해 write transaction을 매 row 계산마다 2번씩 가해주어야 한다.
                for (int i = 0; i < 2; i++) {
                    for (int ch = 0; ch < NUM_CHANNEL; ch++) {
                        Address addr(ch, 0, 0, 0, row_offset, i); // at this time, col does not matter
                        uint64_t hex_addr = ReverseAddressMapping(addr);
			 // putting 2 write command in bank0 at same row is the only thing that matters
                        TryAddTransaction(bank0_base_row + hex_addr, true, data_temp_);
                    }
                }

                Barrier(); // 1 row finished, wait till all the row operations are done
            }
        }
        memory_system_.SetMode(0); // tell memory sysem to change the controllers mode to SB mode
        memory_system_.SetWriteBufferThreshold(-1); // change threshold to default value

    }

    // Read PIM computation result from physical memory
    // Z를 읽기만 하면 된다.
    void AddTransactionGenerator::GetResult() {
        // Mode transition: BG -> SB
#ifdef debug_mode
        std::cout << "HOST:\t[4] AB -> SB \n";
#endif
        for (int ch = 0; ch < NUM_CHANNEL; ch++) {
            Address addr(ch, 0, 0, 0, MAP_SBMR, 0);
            uint64_t hex_addr = ReverseAddressMapping(addr);
            TryAddTransaction(hex_addr, false, data_temp_);
        }
        Barrier();

        uint64_t strided_size = Ceiling(n_ * UNIT_SIZE, SIZE_WORD * NUM_BANK);
        // Read output data z
#ifdef debug_mode
        std::cout << "\nHOST:\tRead output data z\n";
#endif
        for (int offset = 0; offset < strided_size; offset += SIZE_WORD) {
            uint64_t address;
            address = ADDR_CONV1(addr_z_ + offset);
            TryAddTransaction(address, false, z_ + offset);
        }
        Barrier();
    }

    // Calculate error between the result of PIM computation and actual answer
    void AddTransactionGenerator::CheckResult() {
        int err = 0;
        uint16_t sum;
        for (int i = 0; i < n_; i++) {
            sum = ((uint16_t*)x_)[i] + ((uint16_t*)y_)[i];
            err += ABS(((uint16_t*)z_)[i] - sum);    
            //std::cout << "real: " << sum << " result: " << ((uint16_t*)z_)[i] << std::endl;
        }

        std::cout << "ERROR : " << err << std::endl;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////////////////////////
    /*
    
    // Initialize variables and ukernel
    void MulTransactionGenerator::Initialize() {
        // base address of operands
        addr_x_ = 0;
        addr_y_ = Ceiling(n_ * UNIT_SIZE, SIZE_ROW * NUM_BANK);
        addr_z_ = addr_y_ + Ceiling(n_ * UNIT_SIZE, SIZE_ROW * NUM_BANK);

        // total access size of one operand in one ukernel cycle
        ukernel_access_size_ = SIZE_WORD * 8 * NUM_BANK;

        // number of total ukernel cycles to run the whole computation
        ukernel_count_per_pim_ = Ceiling(n_ * UNIT_SIZE, ukernel_access_size_)
            / ukernel_access_size_;

        // Define ukernel
        ukernel_mul_ = (uint32_t*)malloc(sizeof(uint32_t) * 32);
        ukernel_mul_[0] = 0b01000010000000001000000000000000; // MOV(AAM0)  GRF_A  BANK
        ukernel_mul_[1] = 0b00010000000001000000100000000111; // JUMP       -1     7
        ukernel_mul_[2] = 0b10010010000010001000000000000000; // MUL(AAM0)  GRF_A  BANK  GRF_A
        ukernel_mul_[3] = 0b00010000000001000000100000000111; // JUMP       -1     7
        ukernel_mul_[4] = 0b01000000010000001000000000000000; // MOV(AAM0)  BANK   GRF_A
        ukernel_mul_[5] = 0b00010000000001000000100000000111; // JUMP       -1     7
        ukernel_mul_[6] = 0b01000010000000001000000000000000; // MOV(AAM0)  GRF_A  BANK
        ukernel_mul_[7] = 0b00010000000001000000100000000111; // JUMP       -1     7
        ukernel_mul_[8] = 0b10010010000010001000000000000000; // MUL(AAM0)  GRF_A  BANK  GRF_A
        ukernel_mul_[9] = 0b00010000000001000000100000000111; // JUMP       -1     7
        ukernel_mul_[10] = 0b01000000010000001000000000000000; // MOV(AAM0)  BANK   GRF_A
        ukernel_mul_[11] = 0b00010000000001000000100000000111; // JUMP       -1     7
        ukernel_mul_[12] = 0b00100000000000000000000000000000; // EXIT
    }

    // Write operand data and μkernel to physical memory and PIM registers
    void MulTransactionGenerator::SetData() {
        // strided size of one operand with one computation part(minimum)
        uint64_t strided_size = Ceiling(n_ * UNIT_SIZE, SIZE_WORD * NUM_BANK);

#ifdef debug_mode
        std::cout << "HOST:\tSet input data\n";
#endif
        // Write input data x to physical memory
        for (int offset = 0; offset < strided_size; offset += SIZE_WORD)
            TryAddTransaction(addr_x_ + offset, true, x_ + offset);

        // Write input data y to physical memory
        for (int offset = 0; offset < strided_size; offset += SIZE_WORD)
            TryAddTransaction(addr_y_ + offset, true, y_ + offset);
        Barrier();

        // Mode transition: SB -> AB
#ifdef debug_mode
        std::cout << "\nHOST:\t[1] SB -> AB \n";
#endif
        for (int ch = 0; ch < NUM_CHANNEL; ch++) {
            Address addr(ch, 0, 0, 0, MAP_ABMR, 0);
            uint64_t hex_addr = ReverseAddressMapping(addr);
            TryAddTransaction(hex_addr, false, data_temp_);
        }
        Barrier();

        // Program μkernel into CRF register
#ifdef debug_mode
        std::cout << "\nHOST:\tProgram μkernel \n";
#endif
        for (int ch = 0; ch < NUM_CHANNEL; ch++) {
            for (int co = 0; co < 4; co++) {
                Address addr(ch, 0, 0, 0, MAP_CRF, co);
                uint64_t hex_addr = ReverseAddressMapping(addr);
                TryAddTransaction(hex_addr, true, (uint8_t*)&ukernel_mul_[co * 8]);
            }
        }
        Barrier();
    }

    // Execute PIM computation
    void MulTransactionGenerator::Execute() {
        // ro : row index in bank
        // co_o(column_out) : column index counting by 8 words in bank
        // co_i(column_in) : column index counting by word in co_o(column_out)
        for (int ro = 0; ro * NUM_WORD_PER_ROW / 8 < ukernel_count_per_pim_; ro++) {
            for (int co_o = 0; co_o < NUM_WORD_PER_ROW / 8; co_o++) {
                // Check that all data operations have been completed
                if (ro * NUM_WORD_PER_ROW / 8 + co_o > ukernel_count_per_pim_)
                    break;

                // Mode transition: AB -> AB-PIM
#ifdef debug_mode
                std::cout << "HOST:\t[2] AB -> PIM \n";
#endif
                * data_temp_ |= 1;
                for (int ch = 0; ch < NUM_CHANNEL; ch++) {
                    Address addr(ch, 0, 0, 0, MAP_PIM_OP_MODE, 0);
                    uint64_t hex_addr = ReverseAddressMapping(addr);
                    TryAddTransaction(hex_addr, true, data_temp_);
                }
                //Barrier();


#ifdef debug_mode
                std::cout << "\nHOST:\tExecute μkernel 0-9\n";
#endif
                // Execute ukernel 0-1
                for (int co_i = 0; co_i < 8; co_i++) {
                    uint64_t co = co_o * 8 + co_i;
                    for (int ch = 0; ch < NUM_CHANNEL; ch++) {
                        Address addr(ch, 0, 0, EVEN_BANK, ro, co);
                        uint64_t hex_addr = ReverseAddressMapping(addr);
                        TryAddTransaction(addr_x_ + hex_addr, false, data_temp_);
                    }
                }
                //Barrier();

                // Execute ukernel 2-3
                for (int co_i = 0; co_i < 8; co_i++) {
                    uint64_t co = co_o * 8 + co_i;
                    for (int ch = 0; ch < NUM_CHANNEL; ch++) {
                        Address addr(ch, 0, 0, EVEN_BANK, ro, co);
                        uint64_t hex_addr = ReverseAddressMapping(addr);
                        TryAddTransaction(addr_y_ + hex_addr, false, data_temp_);
                    }
                }
                //Barrier();

                // Execute ukernel 4-5
                for (int co_i = 0; co_i < 8; co_i++) {
                    uint64_t co = co_o * 8 + co_i;
                    for (int ch = 0; ch < NUM_CHANNEL; ch++) {
                        Address addr(ch, 0, 0, EVEN_BANK, ro, co);
                        uint64_t hex_addr = ReverseAddressMapping(addr);
                        TryAddTransaction(addr_z_ + hex_addr, true, data_temp_);
                    }
                }
                //Barrier();

                // Execute ukernel 6-7
                for (int co_i = 0; co_i < 8; co_i++) {
                    uint64_t co = co_o * 8 + co_i;
                    for (int ch = 0; ch < NUM_CHANNEL; ch++) {
                        Address addr(ch, 0, 0, ODD_BANK, ro, co);
                        uint64_t hex_addr = ReverseAddressMapping(addr);
                        TryAddTransaction(addr_x_ + hex_addr, false, data_temp_);
                    }
                }
                //Barrier();


                // Execute ukernel 8-9
                for (int co_i = 0; co_i < 8; co_i++) {
                    uint64_t co = co_o * 8 + co_i;
                    for (int ch = 0; ch < NUM_CHANNEL; ch++) {
                        Address addr(ch, 0, 0, ODD_BANK, ro, co);
                        uint64_t hex_addr = ReverseAddressMapping(addr);
                        TryAddTransaction(addr_y_ + hex_addr, false, data_temp_);
                    }
                }
                //Barrier();


                // Execute ukernel 10-11 + AB-PIM -> AB
                // AB-PIM -> AB occurs automatically at the end of the kernel(EXIT)
#ifdef debug_mode
                std::cout << "\nHOST:\tExecute μkernel 10-11 + [3] PIM -> AB \n";
#endif
                for (int co_i = 0; co_i < 8; co_i++) {
                    uint64_t co = co_o * 8 + co_i;
                    for (int ch = 0; ch < NUM_CHANNEL; ch++) {
                        Address addr(ch, 0, 0, ODD_BANK, ro, co);
                        uint64_t hex_addr = ReverseAddressMapping(addr);
                        TryAddTransaction(addr_z_ + hex_addr, true, data_temp_);
                    }
                }
                //Barrier();
            }
        }
        Barrier();
    }

    // Read PIM computation result from physical memory
    void MulTransactionGenerator::GetResult() {
        // Mode transition: AB -> SB
#ifdef debug_mode
        std::cout << "HOST:\t[4] AB -> SB \n";
#endif
        for (int ch = 0; ch < NUM_CHANNEL; ch++) {
            Address addr(ch, 0, 0, 0, MAP_SBMR, 0);
            uint64_t hex_addr = ReverseAddressMapping(addr);
            TryAddTransaction(hex_addr, false, data_temp_);
        }
        Barrier();

        uint64_t strided_size = Ceiling(n_ * UNIT_SIZE, SIZE_WORD * NUM_BANK);
        // Read output data z
#ifdef debug_mode
        std::cout << "\nHOST:\tRead output data z\n";
#endif
        for (int offset = 0; offset < strided_size; offset += SIZE_WORD)
            TryAddTransaction(addr_z_ + offset, false, z_ + offset);
        Barrier();
    }

    // Calculate error between the result of PIM computation and actual answer
    void MulTransactionGenerator::CheckResult() {
        int err = 0;
        float h_err = 0.;
        uint8_t* answer = (uint8_t*)malloc(sizeof(uint16_t) * n_);

        // Calculate actual answer of GEMV
        for (int i = 0; i < n_; i++) {
            half h_x(*reinterpret_cast<half*>(&((uint16_t*)x_)[i]));
            half h_y(*reinterpret_cast<half*>(&((uint16_t*)y_)[i]));
            half h_answer = h_x * h_y;
            ((uint16_t*)answer)[i] = *reinterpret_cast<uint16_t*>(&h_answer);
        }

        // Calculate error
        for (int i = 0; i < n_; i++) {
            half h_answer(*reinterpret_cast<half*>(&((uint16_t*)answer)[i]));
            half h_z(*reinterpret_cast<half*>(&((uint16_t*)z_)[i]));
            h_err += fabs(h_answer - h_z);  // fabs stands for float absolute value
        }
        std::cout << "ERROR : " << h_err << std::endl;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////////////////////////

    // Initialize variables and ukernel
    void GemvTransactionGenerator::Initialize() {
        // TODO(bepo): currently only support m=4096

        addr_A_ = 0;
        addr_y_ = Ceiling(m_ * n_ * UNIT_SIZE, SIZE_ROW * NUM_BANK);

        ukernel_access_size_ = SIZE_WORD * 8 * NUM_BANK;
        ukernel_count_per_pim_ = Ceiling(m_ * n_ * UNIT_SIZE, ukernel_access_size_)
            / ukernel_access_size_;

        // Define ukernel for gemv
        ukernel_gemv_ = (uint32_t*)malloc(sizeof(uint32_t) * 32);
        for (int i = 0; i < 32; i++)
            ukernel_gemv_[i] = 0b00000000000000000000000000000001; // initialize

        ukernel_gemv_[0] = 0b10100100001000001000100000000000; // MAC(AAM)   GRF_B[0]  BANK  SRF_M
        ukernel_gemv_[1] = 0b00010000000001000000100000000111; // JUMP       -1        7
        ukernel_gemv_[2] = 0b00100000000000000000000000000000; // EXIT

        // Define ukernel for reducing output data from ukernel_gemv + write to
        // physical memory
        ukernel_gemv_last_ = (uint32_t*)malloc(sizeof(uint32_t) * 32);
        for (int i = 0; i < 32; i++)
            ukernel_gemv_last_[i] = 0b00000000000000000000000000000000; // initialize

        ukernel_gemv_last_[0] = 0b10100100001000001000100000000000; // MAC(AAM)  GRF_B[0]  BANK  SRF_M
        ukernel_gemv_last_[1] = 0b00010000000001000000100000000111; // JUMP      -1        7
        ukernel_gemv_last_[2] = 0b01000000100000000000000000000000; // MOV       BANK      GRF_B[0]
        ukernel_gemv_last_[3] = 0b00100000000000000000000000000000; // EXIT
    }

    // Write operand data and μkernel to physical memory and PIM registers
    void GemvTransactionGenerator::SetData() {
        uint64_t strided_size = Ceiling(m_ * n_ * UNIT_SIZE, SIZE_WORD * NUM_BANK);

        // Transpose input data A
        A_T_ = (uint8_t*)malloc(sizeof(uint16_t) * m_ * n_);
        for (int m = 0; m < m_; m++) {
            for (int n = 0; n < n_; n++) {
                ((uint16_t*)A_T_)[n * m_ + m] = ((uint16_t*)A_)[m * n_ + n];
            }
        }

#ifdef debug_mode
        std::cout << "HOST:\tSet input data\n";
#endif
        // Write input data A
        for (int offset = 0; offset < strided_size; offset += SIZE_WORD)
            TryAddTransaction(addr_A_ + offset, true, A_T_ + offset);
        Barrier();

        // Mode transition: SB -> AB
#ifdef debug_mode
        std::cout << "\nHOST:\t[1] SB -> AB \n";
#endif
        for (int ch = 0; ch < NUM_CHANNEL; ch++) {
            Address addr(ch, 0, 0, 0, MAP_ABMR, 0);
            uint64_t hex_addr = ReverseAddressMapping(addr);
            TryAddTransaction(hex_addr, false, data_temp_);
        }
        Barrier();
    }

    // Execute PIM computation
    void GemvTransactionGenerator::Execute() {
        ExecuteBank(EVEN_BANK);
        ExecuteBank(ODD_BANK);
        //Barrier();
    }

    // Execute PIM computation of EVEN_BANK or ODD_BANK
    void GemvTransactionGenerator::ExecuteBank(int bank) {
        // Program gemv μkernel
#ifdef debug_mode
        std::cout << "HOST:\tProgram gemv μkernel \n";
#endif
        for (int ch = 0; ch < NUM_CHANNEL; ch++) {
            for (int co = 0; co < 4; co++) {
                Address addr(ch, 0, 0, 0, MAP_CRF, co);
                uint64_t hex_addr = ReverseAddressMapping(addr);
                TryAddTransaction(hex_addr, true, (uint8_t*)&ukernel_gemv_[co * 8]);
            }
        }
        Barrier();

        // Execute for EVEN_BANK or ODD_BANK
        for (int ro = 0; ro * NUM_WORD_PER_ROW / 8 < ukernel_count_per_pim_; ro++) {
            for (int co_o = 0; co_o < NUM_WORD_PER_ROW / 8; co_o++) {
                // SRF_M modify
                std::memcpy(data_temp_ + 16,
                    ((uint16_t*)x_) + (ro * NUM_WORD_PER_ROW + co_o * 8),
                    16);

#ifdef debug_mode
                std::cout << "\nHOST:\tSet Srf\n";
#endif
                for (int ch = 0; ch < NUM_CHANNEL; ch++) {
                    Address addr(ch, 0, 0, bank, MAP_SRF, 0);
                    uint64_t hex_addr = ReverseAddressMapping(addr);
                    TryAddTransaction(hex_addr, true, data_temp_);
                }
                Barrier();

                // if last gemv ukernel to execute, add new gemv ukernel (= ukernel_gemv_last)
                if (ro * NUM_WORD_PER_ROW / 8 + co_o >= ukernel_count_per_pim_ - 1) {
                    for (int ch = 0; ch < NUM_CHANNEL; ch++) {
                        for (int co = 0; co < 4; co++) {
                            Address addr(ch, 0, 0, 0, MAP_CRF, co);
                            uint64_t hex_addr = ReverseAddressMapping(addr);
                            TryAddTransaction(hex_addr, true, (uint8_t*)&ukernel_gemv_last_[co * 8]);
                        }
                    }
                    Barrier();
                }

                // Mode transition: AB -> AB-PIM
#ifdef debug_mode
                std::cout << "\nHOST:\t[2] AB -> PIM \n";
#endif
                * data_temp_ |= 1;
                for (int ch = 0; ch < NUM_CHANNEL; ch++) {
                    Address addr(ch, 0, 0, 0, MAP_PIM_OP_MODE, 0);
                    uint64_t hex_addr = ReverseAddressMapping(addr);
                    TryAddTransaction(hex_addr, true, data_temp_);
                }
                Barrier();

                // Execute ukernel 0-1 + AB-PIM -> AB
#ifdef debug_mode
                std::cout << "\nHOST:\tExecute μkernel 0-1 + [3] PIM -> AB \n";
#endif
                for (int co_i = 0; co_i < 8; co_i++) {
                    uint64_t co = co_o * 8 + co_i;
                    for (int ch = 0; ch < NUM_CHANNEL; ch++) {
                        Address addr(ch, 0, 0, bank, ro, co);
                        uint64_t hex_addr = ReverseAddressMapping(addr);
                        TryAddTransaction(hex_addr, false, data_temp_);
                    }
                }
                Barrier();

                // for the last gemv ukernel, move result to bank
                if (ro * NUM_WORD_PER_ROW / 8 + co_o >= ukernel_count_per_pim_ - 1) {
                    for (int uker = 0; uker < 1; uker++) {
                        for (int ch = 0; ch < NUM_CHANNEL; ch++) {
                            Address addr(ch, 0, 0, bank, 0, 0);
                            uint64_t hex_addr = ReverseAddressMapping(addr);
                            TryAddTransaction(addr_y_ + hex_addr, true, data_temp_);
                        }
                        Barrier();
                    }
                    break;
                }
            }
        }

        // reset GRF_B
#ifdef debug_mode
        std::cout << "\nHOST:\tReset GRF_B\n";
#endif
        uint8_t* zero = (uint8_t*)malloc(WORD_SIZE);
        for (int i = 0; i < WORD_SIZE; i++) zero[i] = 0;
        for (int ch = 0; ch < NUM_CHANNEL; ch++) {
            for (int co = 8; co < 16; co++) {
                Address addr(ch, 0, 0, 0, MAP_GRF, co);
                uint64_t hex_addr = ReverseAddressMapping(addr);
                TryAddTransaction(hex_addr, true, zero);
            }
        }
        Barrier();
    }

    // Read PIM computation result from physical memory
    void GemvTransactionGenerator::GetResult() {
        // Mode transition: AB -> SB
#ifdef debug_mode
        std::cout << "HOST:\t[4] AB -> SB \n";
#endif
        for (int ch = 0; ch < NUM_CHANNEL; ch++) {
            Address addr(ch, 0, 0, 0, MAP_SBMR, 0);
            uint64_t hex_addr = ReverseAddressMapping(addr);
            TryAddTransaction(hex_addr, false, data_temp_);
        }
        Barrier();

        uint64_t strided_size = Ceiling(m_ * UNIT_SIZE, SIZE_WORD * NUM_BANK);
        // Read output data z
#ifdef debug_mode
        std::cout << "\nHOST:\tRead output data z\n";
#endif
        for (int offset = 0; offset < strided_size; offset += SIZE_WORD)
            TryAddTransaction(addr_y_ + offset, false, y_ + offset);
        Barrier();
    }

    // Calculate error between the result of PIM computation and actual answer
    void GemvTransactionGenerator::CheckResult() {
        float h_err = 0.;
        uint8_t* answer = (uint8_t*)malloc(sizeof(uint16_t) * m_);

        for (int m = 0; m < m_; m++) {
            half h_answer(0);
            for (int n = 0; n < n_; n++) {
                half h_A(*reinterpret_cast<half*>(&((uint16_t*)A_)[m * n_ + n]));
                half h_x(*reinterpret_cast<half*>(&((uint16_t*)x_)[n]));
                h_answer = fma(h_A, h_x, h_answer);
            }
            ((uint16_t*)answer)[m] = *reinterpret_cast<uint16_t*>(&h_answer);
        }

        // Calculate error
        for (int m = 0; m < m_; m++) {
            half h_answer(*reinterpret_cast<half*>(&((uint16_t*)answer)[m]));
            half h_y(*reinterpret_cast<half*>(&((uint16_t*)y_)[m]));
            h_err += fabs(h_answer - h_y);  // fabs stands for float absolute value
        }
        std::cout << "ERROR : " << h_err << std::endl;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////////////////////////

    // Initialize variables and ukernel
    void BatchNormTransactionGenerator::Initialize() {
        // base address of operands
        addr_x_ = 0;
        addr_w_ = Ceiling(l_ * f_ * UNIT_SIZE, SIZE_ROW * NUM_BANK);
        addr_y_ = addr_w_ + Ceiling(l_ * f_ * UNIT_SIZE, SIZE_ROW * NUM_BANK);
        addr_z_ = addr_y_ + Ceiling(f_ * UNIT_SIZE, SIZE_ROW * NUM_BANK);

        // total access size of one operand in one ukernel cycle
        ukernel_access_size_ = SIZE_WORD * 8 * NUM_BANK;

        // number of total ukernel cycles to run the whole computation
        ukernel_count_per_pim_ = Ceiling(l_ * f_ * UNIT_SIZE, ukernel_access_size_)
            / ukernel_access_size_;

        // Define ukernel
        ukernel_bn_ = (uint32_t*)malloc(sizeof(uint32_t) * 32);

        ukernel_bn_[0] = 0b01000010000000001000000000000000;  // MOV(AAM) GRF_A BANK
        ukernel_bn_[1] = 0b00010000000001000000100000000111;  // JUMP -1 7
        ukernel_bn_[2] = 0b10010010000010001000000000000000;  // MUL(AAM) GRF_A BANK GRF_A
        ukernel_bn_[3] = 0b00010000000001000000100000000111;  // JUMP -1 7
        ukernel_bn_[4] = 0b10000010000010001000000000000000;  // ADD(AAM) GRF_A BANK GRF_A
        ukernel_bn_[5] = 0b00010000000001000000100000000111;  // JUMP -1 7
        ukernel_bn_[6] = 0b01000000010000001000000000000000;  // MOV BANK GRF_A
        ukernel_bn_[7] = 0b00010000000001000000100000000111;  // JUMP -1 7
        ukernel_bn_[8] = 0b01000010000000001000000000000000;  // MOV(AAM) GRF_A BANK
        ukernel_bn_[9] = 0b00010000000001000000100000000111;  // JUMP -1 7
        ukernel_bn_[10] = 0b10010010000010001000000000000000;  // MUL(AAM) GRF_A BANK GRF_A
        ukernel_bn_[11] = 0b00010000000001000000100000000111;  // JUMP -1 7
        ukernel_bn_[12] = 0b10000010000010001000000000000000;  // ADD(AAM) GRF_A BANK GRF_A
        ukernel_bn_[13] = 0b00010000000001000000100000000111;  // JUMP -1 7 
        ukernel_bn_[14] = 0b01000000010000001000000000000000;  // MOV BANK GRF_A
        ukernel_bn_[15] = 0b00010000000001000000100000000111;  // JUMP -1 7
        ukernel_bn_[16] = 0b00100000000000000000000000000000;  // EXIT
    }

    // Write operand data and μkernel to physical memory and PIM registers
    void BatchNormTransactionGenerator::SetData() {
        // strided size of one operand with one computation part(minimum)
        uint64_t strided_size = Ceiling(l_ * f_ * UNIT_SIZE, SIZE_WORD * NUM_BANK);
        uint64_t strided_size_ = Ceiling(4096 * 8 * UNIT_SIZE, SIZE_WORD * NUM_BANK);

#ifdef debug_mode
        std::cout << "HOST:\tSet input data\n";
#endif
        // Write input data x to physical memory
        for (int offset = 0; offset < strided_size; offset += SIZE_WORD)
            TryAddTransaction(addr_x_ + offset, true, x_ + offset);

        // Write input data y to physical memory
        for (int offset = 0; offset < strided_size_; offset += SIZE_WORD) {
            TryAddTransaction(addr_y_ + offset, true, y_ + offset);
        }

        // Write input data z to physical memory
        for (int offset = 0; offset < strided_size_; offset += SIZE_WORD) {
            TryAddTransaction(addr_z_ + offset, true, z_ + offset);
        }
        Barrier();

        // Mode transition: SB -> AB
#ifdef debug_mode
        std::cout << "\nHOST:\t[1] SB -> AB \n";
#endif
        for (int ch = 0; ch < NUM_CHANNEL; ch++) {
            Address addr(ch, 0, 0, 0, MAP_ABMR, 0);
            uint64_t hex_addr = ReverseAddressMapping(addr);
            TryAddTransaction(hex_addr, false, data_temp_);
        }
        Barrier();

        // Program μkernel into CRF register
#ifdef debug_mode
        std::cout << "\nHOST:\tProgram μkernel \n";
#endif
        for (int ch = 0; ch < NUM_CHANNEL; ch++) {
            for (int co = 0; co < 4; co++) {
                Address addr(ch, 0, 0, 0, MAP_CRF, co);
                uint64_t hex_addr = ReverseAddressMapping(addr);
                TryAddTransaction(hex_addr, true, (uint8_t*)&ukernel_bn_[co * 8]);
            }
        }
        Barrier();
    }

    // Execute PIM computation
    void BatchNormTransactionGenerator::Execute() {
        // ro : row index in bank
        // co_o(column_out) : column index counting by 8 words in bank
        // co_i(column_in) : column index counting by word in co_o(column_out)
        for (int ro = 0; ro * NUM_WORD_PER_ROW / 8 < ukernel_count_per_pim_; ro++) {
            for (int co_o = 0; co_o < NUM_WORD_PER_ROW / 8; co_o++) {
                // Check that all data operations have been completed
                if (ro * NUM_WORD_PER_ROW / 8 + co_o > ukernel_count_per_pim_)
                    break;

                // Mode transition: AB -> AB-PIM
#ifdef debug_mode
                std::cout << "HOST:\t[2] AB -> PIM \n";
#endif
                * data_temp_ |= 1;
                for (int ch = 0; ch < NUM_CHANNEL; ch++) {
                    Address addr(ch, 0, 0, 0, MAP_PIM_OP_MODE, 0);
                    uint64_t hex_addr = ReverseAddressMapping(addr);
                    TryAddTransaction(hex_addr, true, data_temp_);
                }
                Barrier();

#ifdef debug_mode
                std::cout << "\nHOST:\tExecute μkernel 0-15\n";
#endif
                // Execute ukernel 0-1
                for (int co_i = 0; co_i < 8; co_i++) {
                    uint64_t co = co_o * 8 + co_i;
                    for (int ch = 0; ch < NUM_CHANNEL; ch++) {
                        Address addr(ch, 0, 0, EVEN_BANK, ro, co);
                        uint64_t hex_addr = ReverseAddressMapping(addr);
                        TryAddTransaction(addr_x_ + hex_addr, false, data_temp_);
                    }
                }
                Barrier();

                // Execute ukernel 2-3
                for (int co_i = 0; co_i < 8; co_i++) {
                    uint64_t co = co_o * 8 + co_i;
                    for (int ch = 0; ch < NUM_CHANNEL; ch++) {
                        Address addr(ch, 0, 0, EVEN_BANK, ro, co_i);
                        uint64_t hex_addr = ReverseAddressMapping(addr);
                        TryAddTransaction(addr_y_ + hex_addr, false, data_temp_);
                    }
                }
                Barrier();

                // Execute ukernel 4-5
                for (int co_i = 0; co_i < 8; co_i++) {
                    uint64_t co = co_o * 8 + co_i;
                    for (int ch = 0; ch < NUM_CHANNEL; ch++) {
                        Address addr(ch, 0, 0, EVEN_BANK, ro, co_i);
                        //std::cout << co << " " << co_o << "\n";
                        uint64_t hex_addr = ReverseAddressMapping(addr);
                        TryAddTransaction(addr_z_ + hex_addr, false, data_temp_);
                    }
                }
                Barrier();

                // Execute ukernel 6-7
                for (int co_i = 0; co_i < 8; co_i++) {
                    uint64_t co = co_o * 8 + co_i;
                    for (int ch = 0; ch < NUM_CHANNEL; ch++) {
                        Address addr(ch, 0, 0, EVEN_BANK, ro, co);
                        uint64_t hex_addr = ReverseAddressMapping(addr);
                        TryAddTransaction(addr_w_ + hex_addr, true, data_temp_);
                    }
                }
                Barrier();

                // Execute ukernel 8-9
                for (int co_i = 0; co_i < 8; co_i++) {
                    uint64_t co = co_o * 8 + co_i;
                    for (int ch = 0; ch < NUM_CHANNEL; ch++) {
                        Address addr(ch, 0, 0, ODD_BANK, ro, co);
                        uint64_t hex_addr = ReverseAddressMapping(addr);
                        TryAddTransaction(addr_x_ + hex_addr, false, data_temp_);
                    }
                }
                Barrier();

                // Execute ukernel 10-11
                for (int co_i = 0; co_i < 8; co_i++) {
                    uint64_t co = co_o * 8 + co_i;
                    for (int ch = 0; ch < NUM_CHANNEL; ch++) {
                        Address addr(ch, 0, 0, ODD_BANK, ro, co_i);
                        uint64_t hex_addr = ReverseAddressMapping(addr);
                        TryAddTransaction(addr_y_ + hex_addr, false, data_temp_);
                    }
                }
                Barrier();

                // Execute ukernel 12-13
                for (int co_i = 0; co_i < 8; co_i++) {
                    uint64_t co = co_o * 8 + co_i;
                    for (int ch = 0; ch < NUM_CHANNEL; ch++) {
                        Address addr(ch, 0, 0, ODD_BANK, ro, co_i);
                        uint64_t hex_addr = ReverseAddressMapping(addr);
                        TryAddTransaction(addr_z_ + hex_addr, false, data_temp_);
                    }
                }
                Barrier();

                // Execute ukernel 14-15 + AB-PIM -> AB
                // AB-PIM -> AB occurs automatically at the end of the kernel(EXIT)
                for (int co_i = 0; co_i < 8; co_i++) {
                    uint64_t co = co_o * 8 + co_i;
                    for (int ch = 0; ch < NUM_CHANNEL; ch++) {
                        Address addr(ch, 0, 0, ODD_BANK, ro, co);
                        uint64_t hex_addr = ReverseAddressMapping(addr);
                        TryAddTransaction(addr_w_ + hex_addr, true, data_temp_);
                    }
                }
                Barrier();
            }
        }
        Barrier();
    }

    // Read PIM computation result from physical memory
    void BatchNormTransactionGenerator::GetResult() {
        // Mode transition: AB -> SB
#ifdef debug_mode
        std::cout << "HOST:\t[4] AB -> SB \n";
#endif
        for (int ch = 0; ch < NUM_CHANNEL; ch++) {
            Address addr(ch, 0, 0, 0, MAP_SBMR, 0);
            uint64_t hex_addr = ReverseAddressMapping(addr);
            TryAddTransaction(hex_addr, false, data_temp_);
        }
        Barrier();

        uint64_t strided_size = Ceiling(l_ * f_ * UNIT_SIZE, SIZE_WORD * NUM_BANK);
        // Read output data w
#ifdef debug_mode
        std::cout << "\nHOST:\tRead output data w\n";
#endif
        for (int offset = 0; offset < strided_size; offset += SIZE_WORD)
            TryAddTransaction(addr_w_ + offset, false, w_ + offset);
        Barrier();
    }

    // Calculate error between the result of PIM computation and actual answer
    void BatchNormTransactionGenerator::CheckResult() {
        int err = 0;
        float h_err = 0.;
        uint8_t* answer = (uint8_t*)malloc(sizeof(uint16_t) * l_ * f_);

        // Calculate actual answer of BN
        for (int li = 0; li < l_; li++) {
            for (int fi = 0; fi < f_; fi++) {
                half h_x(*reinterpret_cast<half*>(&((uint16_t*)x_)[f_ * li + fi]));
                half h_y(*reinterpret_cast<half*>(&((uint16_t*)y_)[fi]));
                half h_z(*reinterpret_cast<half*>(&((uint16_t*)z_)[fi]));
                half h_answer = (h_x * h_y) + h_z;
                ((uint16_t*)answer)[f_ * li + fi] = *reinterpret_cast<uint16_t*>(&h_answer);
            }
        }

        // Calculate error
        for (int li = 0; li < l_; li++) {
            for (int fi = 0; fi < f_; fi++) {
                half h_answer(*reinterpret_cast<half*>(&((uint16_t*)answer)[f_ * li + fi]));
                half h_w(*reinterpret_cast<half*>(&((uint16_t*)w_)[f_ * li + fi]));
                //std::cout << li << " " << fi << " " << h_answer << " " << h_w << std::endl;
                h_err += fabs(h_answer - h_w);  // fabs stands for float absolute value
            }
        }
        std::cout << "ERROR : " << h_err << std::endl;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////////////////////////

    // Initialize variables and ukernel
    void LstmTransactionGenerator::Initialize() {
        // TODO(bepo): currently only support m=4096

        addr_x_ = 0;
        addr_y_ = Ceiling(i_f_ * UNIT_SIZE, SIZE_ROW * NUM_BANK);
        addr_h_ = addr_y_ + Ceiling(o_f_ * 4 * UNIT_SIZE, SIZE_ROW * NUM_BANK);
        addr_b_ = addr_h_ + Ceiling(i_f_ * UNIT_SIZE, SIZE_ROW * NUM_BANK);
        addr_Wx_ = addr_b_ + Ceiling(o_f_ * 4 * UNIT_SIZE, SIZE_ROW * NUM_BANK);
        addr_Wh_ = addr_Wx_ + Ceiling(i_f_ * o_f_ * 4 * UNIT_SIZE, SIZE_ROW * NUM_BANK);

        ukernel_access_size_ = SIZE_WORD * 8 * NUM_BANK;
        ukernel_count_per_pim_ = Ceiling(i_f_ * o_f_ * 4 * UNIT_SIZE, ukernel_access_size_)
            / ukernel_access_size_;

        // Define ukernel for gemv
        ukernel_lstm_ = (uint32_t*)malloc(sizeof(uint32_t) * 32);
        for (int i = 0; i < 32; i++)
            ukernel_lstm_[i] = 0b00000000000000000000000000000000; // initialize

        ukernel_lstm_[0] = 0b10100100001000001000100000000000; // MAC(AAM)   GRF_B[0]  BANK  SRF_M
        ukernel_lstm_[1] = 0b00010000000001000000100000000111; // JUMP       -1        7
        ukernel_lstm_[2] = 0b00100000000000000000000000000000; // EXIT

        // Define ukernel for reducing output data from ukernel_gemv + write to
        // physical memory
        ukernel_wr_result_ = (uint32_t*)malloc(sizeof(uint32_t) * 32);
        for (int i = 0; i < 32; i++)
            ukernel_wr_result_[i] = 0b00000000000000000000000000000000; // initialize

        ukernel_wr_result_[0] = 0b10000010100000000000100010000000; // ADD     GRF_A[0]  GRF_B[0]  BANK
        ukernel_wr_result_[1] = 0b01000000100000000000000000000000; // MOV     BANK      GRF_A[0]
        ukernel_wr_result_[2] = 0b00100000000000000000000000000000; // EXIT
    }

    // Write operand data and μkernel to physical memory and PIM registers
    void LstmTransactionGenerator::SetData() {
        uint64_t strided_size_W = Ceiling(i_f_ * o_f_ * 4 * UNIT_SIZE, SIZE_WORD * NUM_BANK);
        uint64_t strided_size_b = Ceiling(o_f_ * 4 * UNIT_SIZE, SIZE_WORD * NUM_BANK);

        // Transpose input data Wx, Wh
        Wx_T_ = (uint8_t*)malloc(sizeof(uint16_t) * i_f_ * o_f_ * 4);
        Wh_T_ = (uint8_t*)malloc(sizeof(uint16_t) * i_f_ * o_f_ * 4);

        for (int i_fi = 0; i_fi < i_f_; i_fi++) {
            for (int o_fi = 0; o_fi < o_f_ * 4; o_fi++) {
                ((uint16_t*)Wx_T_)[o_f_ * 4 * i_fi + o_fi] = ((uint16_t*)Wx_)[i_f_ * o_fi + i_fi];
                ((uint16_t*)Wh_T_)[o_f_ * 4 * i_fi + o_fi] = ((uint16_t*)Wh_)[i_f_ * o_fi + i_fi];
            }
        }

#ifdef debug_mode
        std::cout << "HOST:\tSet input data\n";
#endif
        // Write input data Wx, Wh
        for (int offset = 0; offset < strided_size_W; offset += SIZE_WORD) {
            TryAddTransaction(addr_Wx_ + offset, true, Wx_T_ + offset);
            TryAddTransaction(addr_Wh_ + offset, true, Wh_T_ + offset);
        }

        // Write input data b
        for (int offset = 0; offset < strided_size_b; offset += SIZE_WORD) {
            TryAddTransaction(addr_b_ + offset, true, b_ + offset);
        }
        Barrier();

        // Mode transition: SB -> AB
#ifdef debug_mode
        std::cout << "\nHOST:\t[1] SB -> AB \n";
#endif
        for (int ch = 0; ch < NUM_CHANNEL; ch++) {
            Address addr(ch, 0, 0, 0, MAP_ABMR, 0);
            uint64_t hex_addr = ReverseAddressMapping(addr);
            TryAddTransaction(hex_addr, false, data_temp_);
        }
        Barrier();
    }

    // Execute PIM computation
    void LstmTransactionGenerator::Execute() {
        ExecuteBank(EVEN_BANK, 0);
        ExecuteBank(EVEN_BANK, 1);
        ExecuteBank(ODD_BANK, 0);
        ExecuteBank(ODD_BANK, 1);
        //Barrier();
    }

    // Execute PIM computation of EVEN_BANK or ODD_BANK
    void LstmTransactionGenerator::ExecuteBank(int bank, bool calc_h) {
        // Program lstm μkernel
#ifdef debug_mode
        std::cout << "HOST:\tProgram lstm μkernel \n";
#endif
        for (int ch = 0; ch < NUM_CHANNEL; ch++) {
            for (int co = 0; co < 4; co++) {
                Address addr(ch, 0, 0, 0, MAP_CRF, co);
                uint64_t hex_addr = ReverseAddressMapping(addr);
                TryAddTransaction(hex_addr, true, (uint8_t*)&ukernel_lstm_[co * 8]);
            }
        }
        Barrier();

        // Execute for EVEN_BANK or ODD_BANK
        for (int ro = 0; ro * NUM_WORD_PER_ROW / 8 < ukernel_count_per_pim_; ro++) {
            for (int co_o = 0; co_o < NUM_WORD_PER_ROW / 8; co_o++) {
#ifdef debug_mode
                std::cout << "\nHOST:\tSet Srf\n";
#endif
                // SRF_M modify
                if (!calc_h) {
                    std::memcpy(data_temp_ + 16,
                        ((uint16_t*)x_) + (ro * NUM_WORD_PER_ROW + co_o * 8),
                        16);
                }
                else {
                    std::memcpy(data_temp_ + 16,
                        ((uint16_t*)h_) + (ro * NUM_WORD_PER_ROW + co_o * 8),
                        16);
                }
                for (int ch = 0; ch < NUM_CHANNEL; ch++) {
                    Address addr(ch, 0, 0, bank, MAP_SRF, 0);
                    uint64_t hex_addr = ReverseAddressMapping(addr);
                    TryAddTransaction(hex_addr, true, data_temp_);
                }
                Barrier();

                // Mode transition: AB -> AB-PIM
#ifdef debug_mode
                std::cout << "\nHOST:\t[2] AB -> PIM \n";
#endif
                * data_temp_ |= 1;
                for (int ch = 0; ch < NUM_CHANNEL; ch++) {
                    Address addr(ch, 0, 0, 0, MAP_PIM_OP_MODE, 0);
                    uint64_t hex_addr = ReverseAddressMapping(addr);
                    TryAddTransaction(hex_addr, true, data_temp_);
                }
                Barrier();

                // Execute ukernel 0-1 + AB-PIM -> AB
#ifdef debug_mode
                std::cout << "\nHOST:\tExecute μkernel 0-1 + [3] PIM -> AB \n";
#endif
                for (int co_i = 0; co_i < 8; co_i++) {
                    uint64_t co = co_o * 8 + co_i;
                    for (int ch = 0; ch < NUM_CHANNEL; ch++) {
                        Address addr(ch, 0, 0, bank, ro, co);
                        uint64_t hex_addr = ReverseAddressMapping(addr);
                        if (!calc_h) {
                            TryAddTransaction(addr_Wx_ + hex_addr, false, data_temp_);
                        }
                        else {
                            TryAddTransaction(addr_Wh_ + hex_addr, false, data_temp_);
                        }
                    }
                }
                Barrier();

                // for the last gemv ukernel, move result to bank
                if (ro * NUM_WORD_PER_ROW / 8 + co_o >= ukernel_count_per_pim_)
                    break;
            }
        }

        if (calc_h) {
            // Program wr_result ukernel
#ifdef debug_mode
            std::cout << "\nHOST:\tProgram wr_result μkernel \n";
#endif
            for (int ch = 0; ch < NUM_CHANNEL; ch++) {
                for (int co = 0; co < 4; co++) {
                    Address addr(ch, 0, 0, 0, MAP_CRF, co);
                    uint64_t hex_addr = ReverseAddressMapping(addr);
                    TryAddTransaction(hex_addr, true, (uint8_t*)&ukernel_wr_result_[co * 8]);
                }
            }
            Barrier();

            // Mode transition: AB -> AB-PIM
#ifdef debug_mode
            std::cout << "\nHOST:\t[4] AB -> PIM \n";
#endif
            * data_temp_ |= 1;
            for (int ch = 0; ch < NUM_CHANNEL; ch++) {
                Address addr(ch, 0, 0, 0, MAP_PIM_OP_MODE, 0);
                uint64_t hex_addr = ReverseAddressMapping(addr);
                TryAddTransaction(hex_addr, true, data_temp_);
            }
            Barrier();

            // Execute ukernel 0~1 + AB-PIM -> AB
#ifdef debug_mode
            std::cout << "\nHOST:\tExecute μkernel 0-1 + [5] PIM -> AB \n";
#endif

            for (int ch = 0; ch < NUM_CHANNEL; ch++) {
                Address addr(ch, 0, 0, bank, 0, 0);
                uint64_t hex_addr = ReverseAddressMapping(addr);
                TryAddTransaction(addr_b_ + hex_addr, false, data_temp_);
            }
            Barrier();

            for (int ch = 0; ch < NUM_CHANNEL; ch++) {
                Address addr(ch, 0, 0, bank, 0, 0);
                uint64_t hex_addr = ReverseAddressMapping(addr);
                TryAddTransaction(addr_y_ + hex_addr, true, data_temp_);
            }
            Barrier();

            // reset GRF_B
#ifdef debug_mode
            std::cout << "\nHOST:\tReset GRF_B\n";
#endif
            uint8_t* zero = (uint8_t*)malloc(WORD_SIZE);
            for (int i = 0; i < WORD_SIZE; i++) zero[i] = 0;
            for (int ch = 0; ch < NUM_CHANNEL; ch++) {
                for (int co = 8; co < 16; co++) {
                    Address addr(ch, 0, 0, 0, MAP_GRF, co);
                    uint64_t hex_addr = ReverseAddressMapping(addr);
                    TryAddTransaction(hex_addr, true, zero);
                }
            }
            Barrier();
        }
    }

    // Read PIM computation result from physical memory
    void LstmTransactionGenerator::GetResult() {
        // Mode transition: AB -> SB
#ifdef debug_mode
        std::cout << "HOST:\t[4] AB -> SB \n";
#endif
        for (int ch = 0; ch < NUM_CHANNEL; ch++) {
            Address addr(ch, 0, 0, 0, MAP_SBMR, 0);
            uint64_t hex_addr = ReverseAddressMapping(addr);
            TryAddTransaction(hex_addr, false, data_temp_);
        }
        Barrier();

        uint64_t strided_size = Ceiling(o_f_ * 4 * UNIT_SIZE, SIZE_WORD * NUM_BANK);
        // Read output data y
#ifdef debug_mode
        std::cout << "\nHOST:\tRead output data y\n";
#endif
        for (int offset = 0; offset < strided_size; offset += SIZE_WORD)
            TryAddTransaction(addr_y_ + offset, false, y_ + offset);
        Barrier();
    }

    // Calculate error between the result of PIM computation and actual answer
    void LstmTransactionGenerator::CheckResult() {
        float h_err = 0.;
        uint8_t* answer = (uint8_t*)malloc(sizeof(uint16_t) * o_f_ * 4);

        for (int o_fi = 0; o_fi < o_f_ * 4; o_fi++) {
            half h_answer(0);
            half h_b(*reinterpret_cast<half*>(&((uint16_t*)b_)[o_fi]));
            for (int i_fi = 0; i_fi < i_f_; i_fi++) {
                half h_Wx(*reinterpret_cast<half*>(&((uint16_t*)Wx_)[i_f_ * o_fi + i_fi]));
                half h_x(*reinterpret_cast<half*>(&((uint16_t*)x_)[i_fi]));
                h_answer = fma(h_Wx, h_x, h_answer);
            }
            for (int i_fi = 0; i_fi < i_f_; i_fi++) {
                half h_Wh(*reinterpret_cast<half*>(&((uint16_t*)Wh_)[i_f_ * o_fi + i_fi]));
                half h_h(*reinterpret_cast<half*>(&((uint16_t*)h_)[i_fi]));
                h_answer = fma(h_Wh, h_h, h_answer);
            }
            h_answer = h_answer + h_b;

            ((uint16_t*)answer)[o_fi] = *reinterpret_cast<uint16_t*>(&h_answer);
        }

        // Calculate error
        for (int o_fi = 0; o_fi < o_f_ * 4; o_fi++) {
            half h_answer(*reinterpret_cast<half*>(&((uint16_t*)answer)[o_fi]));
            half h_y(*reinterpret_cast<half*>(&((uint16_t*)y_)[o_fi]));
            //std::cout << o_fi << " " << h_answer << " " << h_y << std::endl;
            h_err += fabs(h_answer - h_y);  // fabs stands for float absolute value
        }
        std::cout << "ERROR : " << h_err << std::endl;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////////////////////////

    // Initialize variables and ukernel
    void LstmPreTransactionGenerator::Initialize() {
        // TODO(bepo): currently only support m=4096
        addr_x_ = 0;
        addr_y_ = Ceiling(o_f_ * 4 * UNIT_SIZE, SIZE_ROW * NUM_BANK);
        addr_h_ = addr_y_ + Ceiling(o_f_ * 4 * UNIT_SIZE, SIZE_ROW * NUM_BANK);
        addr_b_ = addr_h_ + Ceiling(i_f_ * UNIT_SIZE, SIZE_ROW * NUM_BANK);
        addr_Wh_ = addr_b_ + Ceiling(i_f_ * o_f_ * 4 * UNIT_SIZE, SIZE_ROW * NUM_BANK);

        ukernel_access_size_ = SIZE_WORD * 8 * NUM_BANK;
        ukernel_count_per_pim_ = Ceiling(i_f_ * o_f_ * 4 * UNIT_SIZE, ukernel_access_size_)
            / ukernel_access_size_;

        // Define ukernel for gemv
        ukernel_lstm_ = (uint32_t*)malloc(sizeof(uint32_t) * 32);
        for (int i = 0; i < 32; i++)
            ukernel_lstm_[i] = 0b00000000000000000000000000000000; // initialize

        ukernel_lstm_[0] = 0b10100100001000001000100000000000; // MAC(AAM)   GRF_B[0]  BANK  SRF_M
        ukernel_lstm_[1] = 0b00010000000001000000100000000111; // JUMP       -1        7
        ukernel_lstm_[2] = 0b00100000000000000000000000000000; // EXIT

        // Define ukernel for reducing output data from ukernel_gemv + write to
        // physical memory
        ukernel_wr_result_ = (uint32_t*)malloc(sizeof(uint32_t) * 32);
        for (int i = 0; i < 32; i++)
            ukernel_wr_result_[i] = 0b00000000000000000000000000000000; // initialize

        ukernel_wr_result_[0] = 0b10000010100000000000100010000000; // ADD     GRF_A[0]  GRF_B[0]  BANK
        ukernel_wr_result_[1] = 0b10000100010000000000100110000000; // ADD     GRF_B[1]  GRF_A[0]  BANK
        ukernel_wr_result_[2] = 0b01000000100000000000000000010000; // MOV     BANK      GRF_B[1]
        ukernel_wr_result_[3] = 0b00100000000000000000000000000000; // EXIT
    }

    // Write operand data and μkernel to physical memory and PIM registers
    void LstmPreTransactionGenerator::SetData() {
        uint64_t strided_size_W = Ceiling(i_f_ * o_f_ * 4 * UNIT_SIZE, SIZE_WORD * NUM_BANK);
        uint64_t strided_size_b = Ceiling(o_f_ * 4 * UNIT_SIZE, SIZE_WORD * NUM_BANK);

        // Transpose input data Wh
        Wh_T_ = (uint8_t*)malloc(sizeof(uint16_t) * i_f_ * o_f_ * 4);

        for (int i_fi = 0; i_fi < i_f_; i_fi++)
            for (int o_fi = 0; o_fi < o_f_ * 4; o_fi++)
                ((uint16_t*)Wh_T_)[o_f_ * 4 * i_fi + o_fi] = ((uint16_t*)Wh_)[i_f_ * o_fi + i_fi];

#ifdef debug_mode
        std::cout << "HOST:\tSet input data\n";
#endif
        ukernel_wr_result_[0] = 0b10000010100000000000100010000000; // ADD     GRF_A[0]  GRF_B[0]  BANK
        // Write input data Wx, Wh
        for (int offset = 0; offset < strided_size_W; offset += SIZE_WORD) {
            TryAddTransaction(addr_Wh_ + offset, true, Wh_T_ + offset);
        }

        // Write input data b, result data x
        for (int offset = 0; offset < strided_size_b; offset += SIZE_WORD) {
            TryAddTransaction(addr_b_ + offset, true, b_ + offset);
            TryAddTransaction(addr_x_ + offset, true, x_ + offset);
        }
        Barrier();

        // Mode transition: SB -> AB
#ifdef debug_mode
        std::cout << "\nHOST:\t[1] SB -> AB \n";
#endif
        for (int ch = 0; ch < NUM_CHANNEL; ch++) {
            Address addr(ch, 0, 0, 0, MAP_ABMR, 0);
            uint64_t hex_addr = ReverseAddressMapping(addr);
            TryAddTransaction(hex_addr, false, data_temp_);
        }
        Barrier();
    }

    // Execute PIM computation
    void LstmPreTransactionGenerator::Execute() {
        ExecuteBank(EVEN_BANK);
        ExecuteBank(ODD_BANK);
        //Barrier();
    }

    // Execute PIM computation of EVEN_BANK or ODD_BANK
    void LstmPreTransactionGenerator::ExecuteBank(int bank) {
        // Program lstm μkernel
#ifdef debug_mode
        std::cout << "HOST:\tProgram lstm μkernel \n";
#endif
        for (int ch = 0; ch < NUM_CHANNEL; ch++) {
            for (int co = 0; co < 4; co++) {
                Address addr(ch, 0, 0, 0, MAP_CRF, co);
                uint64_t hex_addr = ReverseAddressMapping(addr);
                TryAddTransaction(hex_addr, true, (uint8_t*)&ukernel_lstm_[co * 8]);
            }
        }
        Barrier();

        // Execute for EVEN_BANK or ODD_BANK
        for (int ro = 0; ro * NUM_WORD_PER_ROW / 8 < ukernel_count_per_pim_; ro++) {
            for (int co_o = 0; co_o < NUM_WORD_PER_ROW / 8; co_o++) {
#ifdef debug_mode
                std::cout << "\nHOST:\tSet Srf\n";
#endif
                // SRF_M modify
                std::memcpy(data_temp_ + 16,
                    ((uint16_t*)h_) + (ro * NUM_WORD_PER_ROW + co_o * 8),
                    16);
                for (int ch = 0; ch < NUM_CHANNEL; ch++) {
                    Address addr(ch, 0, 0, bank, MAP_SRF, 0);
                    uint64_t hex_addr = ReverseAddressMapping(addr);
                    TryAddTransaction(hex_addr, true, data_temp_);
                }
                Barrier();

                // Mode transition: AB -> AB-PIM
#ifdef debug_mode
                std::cout << "\nHOST:\t[2] AB -> PIM \n";
#endif
                * data_temp_ |= 1;
                for (int ch = 0; ch < NUM_CHANNEL; ch++) {
                    Address addr(ch, 0, 0, 0, MAP_PIM_OP_MODE, 0);
                    uint64_t hex_addr = ReverseAddressMapping(addr);
                    TryAddTransaction(hex_addr, true, data_temp_);
                }
                Barrier();

                // Execute ukernel 0-1 + AB-PIM -> AB
#ifdef debug_mode
                std::cout << "\nHOST:\tExecute μkernel 0-1 + [3] PIM -> AB \n";
#endif
                for (int co_i = 0; co_i < 8; co_i++) {
                    uint64_t co = co_o * 8 + co_i;
                    for (int ch = 0; ch < NUM_CHANNEL; ch++) {
                        Address addr(ch, 0, 0, bank, ro, co);
                        uint64_t hex_addr = ReverseAddressMapping(addr);
                        TryAddTransaction(addr_Wh_ + hex_addr, false, data_temp_);
                    }
                }
                Barrier();

                // for the last gemv ukernel, move result to bank
                if (ro * NUM_WORD_PER_ROW / 8 + co_o >= ukernel_count_per_pim_)
                    break;
            }
        }

        // Program wr_result ukernel
#ifdef debug_mode
        std::cout << "\nHOST:\tProgram wr_result μkernel \n";
#endif
        for (int ch = 0; ch < NUM_CHANNEL; ch++) {
            for (int co = 0; co < 4; co++) {
                Address addr(ch, 0, 0, 0, MAP_CRF, co);
                uint64_t hex_addr = ReverseAddressMapping(addr);
                TryAddTransaction(hex_addr, true, (uint8_t*)&ukernel_wr_result_[co * 8]);
            }
        }
        Barrier();

        // Mode transition: AB -> AB-PIM
#ifdef debug_mode
        std::cout << "\nHOST:\t[4] AB -> PIM \n";
#endif
        * data_temp_ |= 1;
        for (int ch = 0; ch < NUM_CHANNEL; ch++) {
            Address addr(ch, 0, 0, 0, MAP_PIM_OP_MODE, 0);
            uint64_t hex_addr = ReverseAddressMapping(addr);
            TryAddTransaction(hex_addr, true, data_temp_);
        }
        Barrier();

        // Execute ukernel 0~2 + AB-PIM -> AB
#ifdef debug_mode
        std::cout << "\nHOST:\tExecute μkernel 0-2 + [5] PIM -> AB \n";
#endif

        for (int ch = 0; ch < NUM_CHANNEL; ch++) {
            Address addr(ch, 0, 0, bank, 0, 0);
            uint64_t hex_addr = ReverseAddressMapping(addr);
            TryAddTransaction(addr_b_ + hex_addr, false, data_temp_);
        }
        Barrier();

        for (int ch = 0; ch < NUM_CHANNEL; ch++) {
            Address addr(ch, 0, 0, bank, 0, 0);
            uint64_t hex_addr = ReverseAddressMapping(addr);
            TryAddTransaction(addr_x_ + hex_addr, false, data_temp_);
        }
        Barrier();

        for (int ch = 0; ch < NUM_CHANNEL; ch++) {
            Address addr(ch, 0, 0, bank, 0, 0);
            uint64_t hex_addr = ReverseAddressMapping(addr);
            TryAddTransaction(addr_y_ + hex_addr, true, data_temp_);
        }
        Barrier();

        // reset GRF_B
#ifdef debug_mode
        std::cout << "\nHOST:\tReset GRF_B\n";
#endif
        uint8_t* zero = (uint8_t*)malloc(WORD_SIZE);
        for (int i = 0; i < WORD_SIZE; i++) zero[i] = 0;
        for (int ch = 0; ch < NUM_CHANNEL; ch++) {
            for (int co = 8; co < 16; co++) {
                Address addr(ch, 0, 0, 0, MAP_GRF, co);
                uint64_t hex_addr = ReverseAddressMapping(addr);
                TryAddTransaction(hex_addr, true, zero);
            }
        }
        Barrier();
    }

    // Read PIM computation result from physical memory
    void LstmPreTransactionGenerator::GetResult() {
        // Mode transition: AB -> SB
#ifdef debug_mode
        std::cout << "HOST:\t[4] AB -> SB \n";
#endif
        for (int ch = 0; ch < NUM_CHANNEL; ch++) {
            Address addr(ch, 0, 0, 0, MAP_SBMR, 0);
            uint64_t hex_addr = ReverseAddressMapping(addr);
            TryAddTransaction(hex_addr, false, data_temp_);
        }
        Barrier();

        uint64_t strided_size = Ceiling(o_f_ * 4 * UNIT_SIZE, SIZE_WORD * NUM_BANK);
        // Read output data y
#ifdef debug_mode
        std::cout << "\nHOST:\tRead output data y\n";
#endif
        for (int offset = 0; offset < strided_size; offset += SIZE_WORD)
            TryAddTransaction(addr_y_ + offset, false, y_ + offset);
        Barrier();
    }

    // Calculate error between the result of PIM computation and actual answer
    void LstmPreTransactionGenerator::CheckResult() {
        float h_err = 0.;
        uint8_t* answer = (uint8_t*)malloc(sizeof(uint16_t) * o_f_ * 4);

        for (int o_fi = 0; o_fi < o_f_ * 4; o_fi++) {
            half h_answer(0);
            half h_b(*reinterpret_cast<half*>(&((uint16_t*)b_)[o_fi]));
            half h_x(*reinterpret_cast<half*>(&((uint16_t*)x_)[o_fi]));

            for (int i_fi = 0; i_fi < i_f_; i_fi++) {
                half h_Wh(*reinterpret_cast<half*>(&((uint16_t*)Wh_)[i_f_ * o_fi + i_fi]));
                half h_h(*reinterpret_cast<half*>(&((uint16_t*)h_)[i_fi]));
                h_answer = fma(h_Wh, h_h, h_answer);
            }
            h_answer = h_answer + h_b + h_x;

            ((uint16_t*)answer)[o_fi] = *reinterpret_cast<uint16_t*>(&h_answer);
        }

        // Calculate error
        for (int o_fi = 0; o_fi < o_f_ * 4; o_fi++) {
            half h_answer(*reinterpret_cast<half*>(&((uint16_t*)answer)[o_fi]));
            half h_y(*reinterpret_cast<half*>(&((uint16_t*)y_)[o_fi]));
            //std::cout << o_fi << " " << h_answer << " " << h_y << std::endl;
            h_err += fabs(h_answer - h_y);  // fabs stands for float absolute value
        }
        std::cout << "ERROR : " << h_err << std::endl;
    }



    //iESLAB/////////////////////////////////////////////////////////////////////
    //////////////CCCCCCC/////////PPPPPPPPPPPPP/////////UUU///////////UUU////////
    //////////CCCCCCCCCCCCCCC/////PPP/////////PPPP//////UUU///////////UUU////////
    /////////CCC//////////CCC/////PPP//////////PPPP/////UUU///////////UUU////////
    ////////CCC///////////////////PPP///////////PPP/////UUU///////////UUU////////
    ////////CCC///////////////////PPP/////////PPPP//////UUU///////////UUU////////
    ////////CCC///////////////////PPPPPPPPPPPPP/////////UUU///////////UUU////////
    ////////CCC///////////////////PPP///////////////////UUU///////////UUU////////
    /////////CCC/////////CCCCC////PPP////////////////////UUU/////////UUU/////////
    //////////CCCCCCCCCCCCC///////PPP//////////////////////UUUU///UUUU///////////
    /////////////CCCCCCC//////////PPP/////////////////////////UUUUU//////////////
    ///////////////////////////////////////////////////////////////////KKM//LHY//


    // Initialize variables and ukernel
    void CPUAddTransactionGenerator::Initialize() {
        // base address of operands
        addr_x_ = 0;
        addr_y_ = Ceiling(b_ * n_ * UNIT_SIZE, SIZE_ROW * NUM_BANK);
        addr_z_ = 2 * addr_y_;
    }

    // Execute PIM computation
    void CPUAddTransactionGenerator::Execute() {
        int cnt = 0;
        std::cout << UNIT_SIZE << " " << n_ << " " << UNITS_PER_WORD << std::endl;
        for (int i = 0; i < UNIT_SIZE * b_ * n_; i += WORD_SIZE) {
            TryAddTransaction(addr_x_ + i, false, data_temp_);
            cnt++;
        }

        std::cout << cnt << std::endl;
        cnt = 0;
        for (int i = 0; i < UNIT_SIZE * b_ * n_; i += WORD_SIZE) {
            TryAddTransaction(addr_y_ + i, false, data_temp_);
            cnt++;
        }

        //Barrier();
        std::cout << cnt << std::endl;
        cnt = 0;
        for (int i = 0; i < UNIT_SIZE * b_ * n_; i += WORD_SIZE) {
            TryAddTransaction(addr_z_ + i, true, data_temp_);
            cnt++;
        }
        Barrier();
        std::cout << cnt << std::endl;
    }


    // Initialize variables and ukernel
    void CPUGemvTransactionGenerator::Initialize() {
        // TODO(bepo): currently only support m=4096
        addr_A_ = 0;
        addr_y_ = Ceiling(m_ * n_ * UNIT_SIZE, SIZE_ROW * NUM_BANK);
        addr_x_ = addr_y_ + Ceiling(b_ * m_ * UNIT_SIZE, SIZE_ROW * NUM_BANK);
    }

    // Execute PIM computation
    void CPUGemvTransactionGenerator::Execute() {
        for (int b = 0; b < b_; b++) {
            for (int m = 0; m < m_; m++) {
                for (int n_offset = 0; n_offset < UNIT_SIZE * n_; n_offset += WORD_SIZE) {
                    // AddTransaction A (read)
                    TryAddTransaction(addr_A_ + m * n_ * UNIT_SIZE + n_offset, false, data_temp_);
                }
            }
            Barrier();

            for (int m = 0; m < uint64_t(m_ * miss_ratio_); m++) {
                for (int n_offset = 0; n_offset < UNIT_SIZE * n_; n_offset += WORD_SIZE) {
                    // AddTransaction x (read)
                    // Need to control reuse factor
                    TryAddTransaction(addr_x_ + n_offset, false, data_temp_);
                }
                Barrier();
            }

            for (int m_offset; m_offset < UNIT_SIZE * m_; m_offset += WORD_SIZE) {
                // Addtransaction y (write)
                TryAddTransaction(addr_y_ + m_offset, true, data_temp_);
            }
            Barrier();
        }
    }


    // Initialize variables and ukernel
    void CPUBatchNormTransactionGenerator::Initialize() {
        addr_x_ = 0;
        addr_w_ = Ceiling(l_ * f_ * UNIT_SIZE, SIZE_ROW * NUM_BANK);
        addr_y_ = addr_w_ + Ceiling(l_ * f_ * UNIT_SIZE, SIZE_ROW * NUM_BANK);
        addr_z_ = addr_y_ + Ceiling(l_ * UNIT_SIZE, SIZE_ROW * NUM_BANK);
    }

    // Execute PIM computation
    void CPUBatchNormTransactionGenerator::Execute() {
        for (int b = 0; b < b_; b++) {
            for (int li = 0; li < l_; li++) {
                for (int f_offset = 0; f_offset < UNIT_SIZE * f_; f_offset += WORD_SIZE) {
                    // AddTransaction x (read)
                    TryAddTransaction(addr_x_ + f_ * li * UNIT_SIZE + f_offset, false, data_temp_);
                }
            }
            Barrier();

            // 1 : cold miss, 0 : can reuse at first
#if 1
            for (int f_offset = 0; f_offset < UNIT_SIZE * f_; f_offset += WORD_SIZE) {
                // AddTransaction y (read)
                // Need to control reuse factor
                TryAddTransaction(addr_y_ + f_offset, false, data_temp_);
            }
            Barrier();
#endif
            for (int li = 0; li < uint64_t(l_ * miss_ratio_); li++) {
                for (int f_offset = 0; f_offset < UNIT_SIZE * f_; f_offset += WORD_SIZE) {
                    // AddTransaction y (read)
                    // Need to control reuse factor
                    TryAddTransaction(addr_y_ + f_offset, false, data_temp_);
                }
                Barrier();
            }

            for (int li = 0; li < uint64_t(l_ * miss_ratio_); li++) {
                for (int f_offset = 0; f_offset < UNIT_SIZE * f_; f_offset += WORD_SIZE) {
                    // AddTransaction z (read)
                    // Need to control reuse factor
                    TryAddTransaction(addr_z_ + f_offset, false, data_temp_);
                }
                Barrier();
            }

            for (int li = 0; li < l_; li++) {
                for (int f_offset = 0; f_offset < UNIT_SIZE * f_; f_offset += WORD_SIZE) {
                    // AddTransaction w (write)
                    TryAddTransaction(addr_w_ + f_ * li * UNIT_SIZE + f_offset, false, data_temp_);
                }
            }
            Barrier();
        }
    }


    // Initialize variables and ukernel
    void CPULstmTransactionGenerator::Initialize() {
        addr_x_ = 0;
        addr_y_ = Ceiling(i_f_ * UNIT_SIZE, SIZE_ROW * NUM_BANK);
        addr_h_ = addr_y_ + Ceiling(o_f_ * 4 * UNIT_SIZE, SIZE_ROW * NUM_BANK);
        addr_b_ = addr_h_ + Ceiling(i_f_ * UNIT_SIZE, SIZE_ROW * NUM_BANK);
        addr_Wx_ = addr_b_ + Ceiling(o_f_ * 4 * UNIT_SIZE, SIZE_ROW * NUM_BANK);
        addr_Wh_ = addr_Wx_ + Ceiling(i_f_ * o_f_ * 4 * UNIT_SIZE, SIZE_ROW * NUM_BANK);
    }

    // Execute PIM computation
    void CPULstmTransactionGenerator::Execute() {
        for (int b = 0; b < b_; b++) {
            for (int i_fi = 0; i_fi < i_f_; i_fi++) {
                for (int o_f_offset = 0; o_f_offset < UNIT_SIZE * o_f_; o_f_offset += WORD_SIZE) {
                    // AddTransaction Wx, Wh (read)
                    TryAddTransaction(addr_Wx_ + o_f_ * i_fi * UNIT_SIZE + o_f_offset, false, data_temp_);
                    TryAddTransaction(addr_Wh_ + o_f_ * i_fi * UNIT_SIZE + o_f_offset, false, data_temp_);
                }
            }

            for (int o_f_offset = 0; o_f_offset < UNIT_SIZE * o_f_; o_f_offset += WORD_SIZE) {
                // AddTransaction A (read)
                TryAddTransaction(addr_b_ + o_f_offset, false, data_temp_);
            }
            Barrier();

            // >> cold miss <<
#if 1
            for (int o_f_offset = 0; o_f_offset < UNIT_SIZE * o_f_; o_f_offset += WORD_SIZE) {
                // AddTransaction x, h (read)
                // Need to control reuse factor
                TryAddTransaction(addr_x_ + o_f_offset, false, data_temp_);
                TryAddTransaction(addr_h_ + o_f_offset, false, data_temp_);
            }
            Barrier();
#endif

            for (int i_fi = 0; i_fi < uint64_t(i_f_ * miss_ratio_); i_fi++) {
                for (int o_f_offset = 0; o_f_offset < UNIT_SIZE * o_f_; o_f_offset += WORD_SIZE) {
                    // AddTransaction x, h (read)
                    // Need to control reuse factor
                    TryAddTransaction(addr_x_ + o_f_offset, false, data_temp_);
                    TryAddTransaction(addr_h_ + o_f_offset, false, data_temp_);
                }
                Barrier();
            }

            for (int o_f_offset = 0; o_f_offset < UNIT_SIZE * o_f_; o_f_offset += WORD_SIZE) {
                // AddTransaction y (write)
                TryAddTransaction(addr_y_ + o_f_offset, true, data_temp_);
            }
            Barrier();
        }
    }
    */
}  // namespace dramsim3
