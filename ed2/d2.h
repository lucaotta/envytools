#ifndef ED2_H
#define ED2_H

#define ED2_WORDLEN 16
#define ED2_MAXVARS 4
#define ED2_MAXINSNS 8
#define ED2_MAXPREFIXES 4
#define ED2_MAXMODS 16
#define ED2_MAXOPERANDS 16

struct ed2_sbf {
	int pos;
	int len;
};

struct ed2_bf {
	struct ed2_sbf sbf[2];
	enum {
		BF_UNSIGNED,
		BF_SIGNED,
		BF_SLIGHTLY_SIGNED,
		BF_ULTRASIGNED,
		BF_LUT,
	} mode;
	int shr;
	int pcrel;
	uint64_t addend;
	uint64_t pospreadd; // <3 xtensa...
	uint64_t *lut;
	int wrapok;
};

struct ed2_word {
	uint64_t w[ED2_MAXOPLEN/8];
};

struct ed2_varm {
	struct ed2_word word[2];
	int varmask[ED2_MAXVARS];
};

struct ed2_opcode {
	struct ed2_word word;
	int len;
};

struct ed2_isa {
	struct ed2_form *forms;
	int formsnum;
	int maxoplen;
	int piecelen;
	int poslen;
};

struct ed2_form {
	struct ed2_varm varm;
	int oplen;
	struct ed2_insn *insns[ED2_MAXINSNS];
	int insnsnum;
};

struct ed2_insn {
	struct ed2_varm varm;
	struct ed2_prefix *prefixes[ED2_MAXPREFIXES];
	char *name;
	struct ed2_mod *mods[ED2_MAXMODS];
	struct ed2_operand *operands[ED2_MAXOPERANDS];
};

struct ed2_prefix {
	struct ed2_prefform *prefforms;
	int prefformsnum;
};

struct ed2_prefform {
	struct ed_varm varm;
	char *name;
	struct ed2_operand *operand;
};

struct ed2_mod {
	struct ed2_modform *modforms;
	int modformsnum;
};

struct ed2_modform {
	struct ed2_varm varm;
	char *name;
};

struct ed2_operand {
	struct ed2_operandform *operandforms;
	int operandformsnum;
};

struct ed2_operandform {
	struct ed2_varm varm;
	struct ed2_mod *mods[ED2_MAXMODS];
	enum ed2_operandtype {
		ED2_OP_IMM,
		ED2_OP_REG,
		ED2_OP_MEM,
		ED2_OP_BF,
		ED2_OP_VECTOR,
		ED2_OP_SUBOP,
		ED2_OP_SWIZZLE,
		ED2_OP_MASK,
	};
	struct ed2_bitfield *bf[2];
	struct ed2_register *reg;
	struct ed2_memory *mem;
	struct ed2_vector *vec;
	struct ed2_operand *subop;
};

#endif
