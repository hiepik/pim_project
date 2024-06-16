#ifndef __PIM_CONFIG_H_
#define __PIM_CONFIG_H_

// set unit size
typedef uint16_t unit_t;

#define UNIT_SIZE		(int)(sizeof(unit_t))
#define WORD_SIZE		32
#define UNITS_PER_WORD	(WORD_SIZE / UNIT_SIZE)

#define CACHE_SIZE		8 * (UNITS_PER_WORD * UNIT_SIZE)
#define SRF_SIZE		(UNITS_PER_WORD * UNIT_SIZE)
#define ACC_SIZE		2 * (UNITS_PER_WORD * UNIT_SIZE)


enum class PIM_OPERATION {
	JUMP = 0,
	NOP,
	EXIT,
	LD,
	ADD,
	MUL,
	BN,
	GEMV,
	ST
};

// 528sumin add command and src_
class PimInstruction {
public:
	PimInstruction():
		PIM_OP(PIM_OPERATION::NOP),
		dst_(-1),
		src_(0),
		imm0_(0),
		imm1_(0) {}
		
	PimInstruction(PIM_OPERATION pim_op, int dst, unsigned src, int imm0 = 0, int imm1 = 0) :
		PIM_OP(pim_op),
		dst_(dst),
		src_(src),
		imm0_(imm0),
		imm1_(imm1) {}

	PIM_OPERATION PIM_OP;
	int dst_;
	unsigned src_;
	int imm0_;
	int imm1_;
};


#endif // __PIM_CONFIG_H
