#include "ed2a.h"

static void fun (struct ed2a_insn *insn, void *parm) {
	FILE *ofile = parm;
	ed2a_print_insn(insn, ofile, &ed2a_def_colors);
}

int main() {
//	ed2a_print_file(ed2a_read_file(stdin, 0, 0), stdout);
	ed2a_read_file(stdin, fun, stdout);
	return 0;
}
