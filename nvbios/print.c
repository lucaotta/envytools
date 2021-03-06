#include "bios.h"
#include <stdio.h>

void dump_hex_script (struct envy_bios *bios, FILE *out, unsigned int start, unsigned int length) {
	int cnt = 0;
	while (length) {
		unsigned int i, len = length;
		if (len > 16) len = 16;
		if (cnt)
			fprintf (out, "     ");
		else
			fprintf (out, "%04x:", start);
		for (i = 0; i < len; i++)
			fprintf(out, " %02x", bios->data[start+i]);
		start += len;
		length -= len;
		if (length) {
			fprintf(out, "\n");
		} else {
			while (len < 16) {
				fprintf(out, "   ");
				len++;
			}
			fprintf(out, "  ");
		}
		cnt++;
	}
}

void envy_bios_dump_hex (struct envy_bios *bios, FILE *out, unsigned int start, unsigned int length) {
	if (start + length > bios->length) {
		fprintf(out, "ERR: Cannot dump 0x%x bytes at 0x%x: OOB\n", length, start);
		if (start < bios->length) {
			length = bios->length - start;
		} else {
			return;
		}
	}
	while (length) {
		unsigned int i, len = length;
		if (len > 16) len = 16;
		fprintf (out, "%04x:", start);
		for (i = 0; i < len; i++)
			fprintf(out, " %02x", bios->data[start+i]);
		fprintf(out, "\n");
		start += len;
		length -= len;
	}
}

static void print_pcir(struct envy_bios *bios, FILE *out) {
	int i;
	fprintf(out, "BIOS size 0x%x [orig: 0x%x], %d valid parts:\n", bios->length, bios->origlength, bios->partsnum);
	for (i = 0; i < bios->partsnum; i++) {
		fprintf(out, "\n");
		if (bios->parts[i].pcir_code_type == 0) {
			fprintf(out, "BIOS part %d at 0x%x size 0x%x [init: 0x%x]. Sig:\n", i, bios->parts[i].start, bios->parts[i].length, bios->parts[i].init_length);
			envy_bios_dump_hex(bios, out, bios->parts[i].start, 3);
			if (!bios->parts[i].chksum_pass)
				fprintf(out, "WARN: checksum fail\n");
		} else {
			fprintf(out, "BIOS part %d at 0x%x size 0x%x. Sig:\n", i, bios->parts[i].start, bios->parts[i].length);
			envy_bios_dump_hex(bios, out, bios->parts[i].start, 2);
		}
		fprintf(out, "PCIR [rev 0x%02x]:\n", bios->parts[i].pcir_rev);
		envy_bios_dump_hex(bios, out, bios->parts[i].start + bios->parts[i].pcir_offset, bios->parts[i].pcir_len);
		fprintf(out, "PCI device: %04x:%04x, class %02x%02x%02x\n", bios->parts[i].pcir_vendor, bios->parts[i].pcir_device, bios->parts[i].pcir_class[2], bios->parts[i].pcir_class[1], bios->parts[i].pcir_class[0]);
		if (bios->parts[i].pcir_vpd)
			fprintf(out, "VPD: %x\n", bios->parts[i].pcir_vpd);
		fprintf(out, "Code type 0x%02x, rev 0x%04x\n", bios->parts[i].pcir_code_type, bios->parts[i].pcir_code_rev);
		fprintf(out, "PCIR indicator: %02x\n", bios->parts[i].pcir_indi);
	}
	if (bios->broken_part)
		fprintf(out, "\nWARN: Couldn't read part %d!\n", bios->partsnum);
	fprintf(out, "\n");
}

static void print_bmp_nv03(struct envy_bios *bios, FILE *out) {
	fprintf(out, "\n");
	fprintf(out, "BMP %02x.%02x at 0x%x\n", bios->bmp_ver_major, bios->bmp_ver_minor, bios->bmp_offset);
	envy_bios_dump_hex(bios, out, bios->bmp_offset, bios->bmp_length);
	fprintf(out, "x86 mode pointer: 0x%x\n", bios->mode_x86);
	fprintf(out, "x86 init pointer: 0x%x\n", bios->init_x86);
	fprintf(out, "init script pointer: 0x%x\n", bios->init_script);
}

static void print_nv03_init_script(struct envy_bios *bios, FILE *out) {
	unsigned offset = bios->init_script;
	unsigned len;
	uint8_t op;
	uint8_t arg8_0, arg8_1, arg8_2;
	uint16_t arg16_0;
	uint32_t arg32_0, arg32_1, arg32_2;
	int err = 0;
	fprintf(out, "\n");
	fprintf(out, "Init script at 0x%x:\n", offset);
	while (1) {
		if (bios_u8(bios, offset, &op)) {
			ENVY_BIOS_ERR("Init script out of bounds!\n");
			return;
		}
		switch (op) {
		case 0x6e:
			len = 13;
			err |= bios_u32(bios, offset+1, &arg32_0);
			err |= bios_u32(bios, offset+5, &arg32_1);
			err |= bios_u32(bios, offset+9, &arg32_2);
			if (err) {
				ENVY_BIOS_ERR("Init script out of bounds!\n");
				return;
			}
			dump_hex_script(bios, out, offset, len);
			fprintf(out, "\tMMIO_MASK 0x%06x &= 0x%08x |= 0x%08x\n", arg32_0, arg32_1, arg32_2);
			break;
		case 0x7a:
			len = 9;
			err |= bios_u32(bios, offset+1, &arg32_0);
			err |= bios_u32(bios, offset+5, &arg32_1);
			if (err) {
				ENVY_BIOS_ERR("Init script out of bounds!\n");
				return;
			}
			dump_hex_script(bios, out, offset, len);
			fprintf(out, "\tMMIO_WR 0x%06x <= 0x%08x\n", arg32_0, arg32_1);
			break;
		case 0x77:
			len = 7;
			err |= bios_u32(bios, offset+1, &arg32_0);
			err |= bios_u16(bios, offset+5, &arg16_0);
			if (err) {
				ENVY_BIOS_ERR("Init script out of bounds!\n");
				return;
			}
			dump_hex_script(bios, out, offset, len);
			fprintf(out, "\tMMIO_WR16 0x%06x <= 0x%04x\n", arg32_0, arg16_0);
			break;
		case 0x79:
			len = 13;
			err |= bios_u32(bios, offset+1, &arg32_0);
			err |= bios_u32(bios, offset+5, &arg32_1);
			err |= bios_u32(bios, offset+9, &arg32_2);
			if (err) {
				ENVY_BIOS_ERR("Init script out of bounds!\n");
				return;
			}
			dump_hex_script(bios, out, offset, len);
			fprintf(out, "\tMMIO_WR_CRYSTAL %06x <= %08x / %08x\n", arg32_0, arg32_1, arg32_2);
			break;
		case 0x74:
			len = 3;
			err |= bios_u16(bios, offset+1, &arg16_0);
			if (err) {
				ENVY_BIOS_ERR("Init script out of bounds!\n");
				return;
			}
			dump_hex_script(bios, out, offset, len);
			fprintf(out, "\tUSLEEP %d\n", arg16_0);
			break;
		case 0x69:
			len = 5;
			err |= bios_u16(bios, offset+1, &arg16_0);
			err |= bios_u8(bios, offset+3, &arg8_0);
			err |= bios_u8(bios, offset+4, &arg8_1);
			if (err) {
				ENVY_BIOS_ERR("Init script out of bounds!\n");
				return;
			}
			dump_hex_script(bios, out, offset, len);
			fprintf(out, "\tIO_MASK 0x%04x &= 0x%02x |= 0x%02x\n", arg16_0, arg8_0, arg8_1);
			break;
		case 0x78:
			len = 6;
			err |= bios_u16(bios, offset+1, &arg16_0);
			err |= bios_u8(bios, offset+3, &arg8_0);
			err |= bios_u8(bios, offset+4, &arg8_1);
			err |= bios_u8(bios, offset+5, &arg8_2);
			if (err) {
				ENVY_BIOS_ERR("Init script out of bounds!\n");
				return;
			}
			dump_hex_script(bios, out, offset, len);
			fprintf(out, "\tIOIDX_MASK 0x%04x[0x%02x] &= 0x%02x |= 0x%02x\n", arg16_0, arg8_0, arg8_1, arg8_2);
			break;
		case 0x6d:
			len = 2;
			err |= bios_u8(bios, offset+1, &arg8_0);
			if (err) {
				ENVY_BIOS_ERR("Init script out of bounds!\n");
				return;
			}
			dump_hex_script(bios, out, offset, len);
			fprintf(out, "\tIF_MEM_SIZE 0x%02x\n", arg8_0);
			break;
		case 0x73:
			len = 9;
			err |= bios_u32(bios, offset+1, &arg32_0);
			err |= bios_u32(bios, offset+5, &arg32_1);
			if (err) {
				ENVY_BIOS_ERR("Init script out of bounds!\n");
				return;
			}
			dump_hex_script(bios, out, offset, len);
			fprintf(out, "\tIF_STRAPS & %08x == %08x\n", arg32_0, arg32_1);
			break;
		case 0x72:
			len = 1;
			dump_hex_script(bios, out, offset, len);
			fprintf(out, "\tRESUME\n");
			break;
		case 0x63:
			len = 1;
			dump_hex_script(bios, out, offset, len);
			fprintf(out, "\tCOMPUTE_MEM\n");
			break;
		case 0x71:
			len = 1;
			dump_hex_script(bios, out, offset, len);
			fprintf(out, "\tQUIT\n");
			return;
		default:
			len = 1;
			dump_hex_script(bios, out, offset, len);
			ENVY_BIOS_ERR("Unknown op 0x%02x in init script!\n", op);
			return;
		}
		offset += len;
	}
}

void envy_bios_print (struct envy_bios *bios, FILE *out, unsigned mask) {
	if (mask & ENVY_BIOS_PRINT_PCIR)
		print_pcir(bios, out);
	switch (bios->type) {
	case ENVY_BIOS_TYPE_UNKNOWN:
		fprintf(out, "BIOS type: UNKNOWN!\n");
		break;
	case ENVY_BIOS_TYPE_NV03:
		fprintf(out, "BIOS type: NV03\n");
		if (bios->bmp_length && (mask & ENVY_BIOS_PRINT_BMP_BIT))
			print_bmp_nv03(bios, out);
		if (bios->init_script && (mask & ENVY_BIOS_PRINT_SCRIPTS))
			print_nv03_init_script(bios, out);
		break;
	case ENVY_BIOS_TYPE_NV04:
		fprintf(out, "BIOS type: NV04\n");
		break;
	}
}
