#include <iostream>
#include <random>
#include "./transaction_generator.h"

using namespace dramsim3;

// main code to simulate PIM simulator
int main(int argc, const char** argv) {
    srand(time(NULL));

    //<mod> need to add code to choose operation
    std::string pim_api = "add";
    // have to initiallize config file and output dir
    std::string config_file = "../configs/HBM2_4Gb_test.ini";
    std::string output_dir = "output.txt";


    // Initialize modules of PIM-Simulator
    //  Transaction Generator + DRAMsim3 + PIM Functional Simulator
    std::cout << C_GREEN << "Initializing modules..." << C_NORMAL << std::endl;
    TransactionGenerator* tx_generator;

    // Define operands and Transaction generator for simulating computation

    if (pim_api == "add") {
        //uint64_t n = args::get(add_n_arg);
        uint64_t n = 4096*8;   // have to make code to get n as an input

        // Define input vector x, y
        uint8_t* x = (uint8_t*)malloc(sizeof(uint16_t) * n);
        uint8_t* y = (uint8_t*)malloc(sizeof(uint16_t) * n);
        // Define output vector z
        uint8_t* z = (uint8_t*)malloc(sizeof(uint16_t) * n);

        // Fill input operands with random value
        // for debug i filled it with non random value
        for (int i = 0; i < n; i++) {
            ((uint16_t*)x)[i] = (uint16_t)i;
            ((uint16_t*)y)[i] = -(uint16_t)i;
        }

        // Define Transaction generator for ADD computation
        tx_generator = new AddTransactionGenerator(config_file, output_dir,
            n, x, y, z);
    }
    /*
    else if (pim_api == "mul") {
        uint64_t n = args::get(mul_n_arg);

        // Define input vector x, y
        uint8_t *x = (uint8_t *) malloc(sizeof(uint16_t) * n);
        uint8_t *y = (uint8_t *) malloc(sizeof(uint16_t) * n);
        // Define output vector z
        uint8_t *z = (uint8_t *) malloc(sizeof(uint16_t) * n);

        // Fill input operands with random value
        for (int i=0; i< n; i++) {
            half h_x = half(f32rng());
            half h_y = half(f32rng());
            ((uint16_t*)x)[i] = *reinterpret_cast<uint16_t*>(&h_x);
            ((uint16_t*)y)[i] = *reinterpret_cast<uint16_t*>(&h_y);
        }

        // Define Transaction generator for ADD computation
        tx_generator = new MulTransactionGenerator(config_file, output_dir,
                                                   n, x, y, z);
    } else if (pim_api == "gemv") {
        uint64_t m = args::get(gemv_m_arg);
        uint64_t n = args::get(gemv_n_arg);

        // Define input matrix A, vector x
        uint8_t *A = (uint8_t *) malloc(sizeof(uint16_t) * m * n);
        uint8_t *x = (uint8_t *) malloc(sizeof(uint16_t) * n);
        // Define output vector y
        uint8_t *y = (uint8_t *) malloc(sizeof(uint16_t) * m);

        // Fill input operands with random value
        for (int i=0; i< n; i++) {
            half h_x = half(f32rng());
            ((uint16_t*)x)[i] = *reinterpret_cast<uint16_t*>(&h_x);
            for (int j=0; j< m; j++) {
                half h_A = half(f32rng());
                ((uint16_t*)A)[j*n+i] = *reinterpret_cast<uint16_t*>(&h_A);
            }
        }

        // Define Transaction generator for GEMV computation
        tx_generator = new GemvTransactionGenerator(config_file, output_dir,
                                                    m, n, A, x, y);
    } else if (pim_api == "bn") {
        uint64_t l = args::get(bn_l_arg);
        uint64_t f = args::get(bn_f_arg);

        uint64_t num_duplicate = 4096 / f;

        // Define input x, weight y, z
        uint8_t *x = (uint8_t *) malloc(sizeof(uint16_t) * l * f);
        uint8_t *y = (uint8_t *) malloc(sizeof(uint16_t) * 4096 * 8);
        uint8_t *z = (uint8_t *) malloc(sizeof(uint16_t) * 4096 * 8);
        // Define output vector w
        uint8_t *w = (uint8_t *) malloc(sizeof(uint16_t) * l * f);

        // Fill input operands with random value
        for (int fi=0; fi<f; fi++) {
            half h_y = half(f32rng());
            half h_z = half(f32rng());
            for (int coi=0; coi<num_duplicate*8; coi++) {
                ((uint16_t*)y)[fi + coi*f] = *reinterpret_cast<uint16_t*>(&h_y);
                ((uint16_t*)z)[fi + coi*f] = *reinterpret_cast<uint16_t*>(&h_z);
            }
            for (int li=0; li<l; li++) {
                half h_x = half(f32rng());
                ((uint16_t*)x)[li*f + fi] = *reinterpret_cast<uint16_t*>(&h_x);
            }
        }
        // Define Transaction generator for GEMV computation
        tx_generator = new BatchNormTransactionGenerator(config_file, output_dir,
                                                         l, f, x, y, z, w);
    }
    */
    else {
        std::cout << "currently only support add" << std::endl;    
    }

    std::cout << C_GREEN << "Success Module Initialize" << C_NORMAL << "\n\n";

    uint64_t clk;

    // Initialize variables and ukernel
    std::cout << C_GREEN << "Initializing severals..." << C_NORMAL << std::endl;
    clk = tx_generator->GetClk();
    tx_generator->Initialize();
    clk = tx_generator->GetClk() - clk;
    std::cout << C_GREEN << "Success Initialize (" << clk << " cycles)" << C_NORMAL << "\n\n";

    // Write operand data and μkernel to physical memory and PIM registers
    std::cout << C_GREEN << "Setting Data..." << C_NORMAL << "\n";
    clk = tx_generator->GetClk();
    tx_generator->SetData();
    clk = tx_generator->GetClk() - clk;
    std::cout << C_GREEN << "Success SetData (" << clk << " cycles)" << C_NORMAL << "\n\n";

    // Execute PIM computation
    std::cout << C_GREEN << "Executing..." << C_NORMAL << "\n";
    tx_generator->is_print_ = true;
    clk = tx_generator->GetClk();
    tx_generator->start_clk_ = clk;
    tx_generator->Execute();
    clk = tx_generator->GetClk() - clk;
    tx_generator->is_print_ = false;
    std::cout << C_GREEN << "Success Execute (" << clk << " cycles)" << C_NORMAL << "\n\n";

    // Read PIM computation result from physical memory
    std::cout << C_GREEN << "Getting Result..." << C_NORMAL << "\n";
    clk = tx_generator->GetClk();
    tx_generator->GetResult();
    clk = tx_generator->GetClk() - clk;
    std::cout << C_GREEN << "Success GetResult (" << clk << " cycles)" << C_NORMAL << "\n\n";

    // Calculate error between the result of PIM computation and actual answer
    tx_generator->CheckResult();

    tx_generator->PrintStats();

    delete tx_generator;

    return 0;
}
