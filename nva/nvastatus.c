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

// FIXME: to be moved into a proper table
struct StatusRegister register_table[] = {
	{0x04, MAX_CARD_TYPE, 0x3014, false},
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
		int card_type = nva_cards[cnum].card_type;
		if (card_type >= r->card_type_start && card_type < r->card_type_end)
			printf("%08X\n", nva_rd32(cnum, r->address));
	}

	return 0;
}
