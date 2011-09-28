#include "nva.h"
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>

struct StatusRegister {
	int card_type_start;
	int card_type_end;
	int32_t address;
	bool indirect; ///< True if the real value is read by accessing the register indicated into the address.
};

#define MAX_CARD_TYPE (0xFFFF)
#define countof(a) (sizeof(a)/sizeof(*(a)))

struct StatusRegister register_table[] = {
	{0x3, 0x4, 0x4006b0, false},
	{0x4, 0x10, 0x400104, false},
	{0x4, 0x10, 0x400700, false},
	{0x40, 0xc0, 0xa540, false},
	{0x40, 0xc0, 0xa800, false},
	{0x40, 0x84, 0xb048, false},
	{0x40, 0x84, 0xf43c, false},
	{0x50, 0xc0, 0x3214, false},
	{0x50, 0xc0, 0x400300, false},
	{0x50, 0xc0, 0x400380, false},
	{0x50, 0xc0, 0x400384, false},
	{0x50, 0xc0, 0x400388, false},
	{0x50, 0xc0, 0x400504, false},
	{0x50, 0xc0, 0x400700, false},
	{0x50, 0xc0, 0x401804, false},
	{0x98, 0x99, 0x84448, false},
	{0xaa, 0xffff, 0x84448, false},
	{0xc0, 0xffff, 0x180000, false},
	{0xc0, 0xffff, 0x400504, false},
	{0xc0, 0xffff, 0x400700, false},
	{0xc0, 0xffff, 0x409c18, false},
	{0x17, 0x20, 0x1308, false},
	{0x25, 0xc0, 0x1308, false},
	{0x1, 0x50, 0x2400, false},
	{0x3, 0xffff, 0x3014, false},
	{0x3, 0xffff, 0x3214, false},
};

int main(int argc, char **argv) {
	if (nva_init()) {
		fprintf (stderr, "PCI init failure!\n");
		return 1;
	}
	int c;
	int cnum =0;
	int i;

	while ((c = getopt (argc, argv, "c:")) != -1)
		switch (c) {
			case 'c':
				sscanf(optarg, "%d", &cnum);
				break;
		}
	if (cnum >= nva_cardsnum) {
		if (nva_cardsnum)
			fprintf (stderr, "No such card.\n");
		else
			fprintf (stderr, "No cards found.\n");
		return 1;
	}

	for (i = 0; i < countof(register_table); ++i)
	{
		struct StatusRegister *r = &register_table[i];
		int card_type = nva_cards[cnum].chipset;
		if (card_type >= r->card_type_start && card_type < r->card_type_end)
			printf("%08X\n", nva_rd32(cnum, r->address));
	}

	return 0;
}
