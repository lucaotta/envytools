#include "bios.h"

static int parse_pcir (struct envy_bios *bios) {
	bios->length = bios->origlength;
	unsigned int curpos = 0;
	unsigned int num = 0;
	int next = 0;
	do {
		uint16_t pcir_offset;
		uint16_t sig;
		uint32_t pcir_sig;
		uint16_t pcir_ilen;
		uint8_t pcir_indi;
		uint16_t pcir_res;
		next = 0;
		if (bios_u16(bios, curpos, &sig)) {
broken_part:
			bios->broken_part = 1;
			break;
		}
		if (sig != 0xaa55)
			goto broken_part;
		if (bios_u16(bios, curpos + 0x18, &pcir_offset))
			goto broken_part;
		if (bios_u32(bios, curpos+pcir_offset, &pcir_sig))
			goto broken_part;
		if (pcir_sig != 0x52494350)
			goto broken_part;
		if (bios_u16(bios, curpos+pcir_offset+0x10, &pcir_ilen))
			goto broken_part;
		if (bios_u8(bios, curpos+pcir_offset+0x15, &pcir_indi))
			goto broken_part;
		if (bios_u16(bios, curpos+pcir_offset+0x16, &pcir_res))
			goto broken_part;
		if (curpos + pcir_ilen * 0x200 > bios->origlength)
			goto broken_part;
		if (!(pcir_indi & 0x80))
			next = 1;
		curpos += pcir_ilen * 0x200;
		num++;
	} while(next);
	if (!num)
		return -EINVAL;
	bios->partsnum = num;
	bios->parts = calloc(bios->partsnum, sizeof *bios->parts);
	if (!bios->parts)
		return -ENOMEM;
	bios->length = curpos;
	curpos = 0;
	for (num = 0; num < bios->partsnum; num++) {
		uint16_t pcir_offset;
		uint16_t pcir_ilen;
		bios->parts[num].start = curpos;
		bios_u16(bios, curpos + 0x18, &pcir_offset);
		bios_u16(bios, curpos+pcir_offset+4, &bios->parts[num].pcir_vendor);
		bios_u16(bios, curpos+pcir_offset+6, &bios->parts[num].pcir_device);
		bios_u16(bios, curpos+pcir_offset+0x8, &bios->parts[num].pcir_vpd);
		bios_u16(bios, curpos+pcir_offset+0xa, &bios->parts[num].pcir_len);
		bios_u8(bios, curpos+pcir_offset+0xc, &bios->parts[num].pcir_rev);
		bios_u8(bios, curpos+pcir_offset+0xd, &bios->parts[num].pcir_class[0]);
		bios_u8(bios, curpos+pcir_offset+0xe, &bios->parts[num].pcir_class[1]);
		bios_u8(bios, curpos+pcir_offset+0xf, &bios->parts[num].pcir_class[2]);
		bios_u16(bios, curpos+pcir_offset+0x10, &pcir_ilen);
		bios_u16(bios, curpos+pcir_offset+0x12, &bios->parts[num].pcir_code_rev);
		bios_u8(bios, curpos+pcir_offset+0x14, &bios->parts[num].pcir_code_type);
		bios_u8(bios, curpos+pcir_offset+0x15, &bios->parts[num].pcir_indi);
		bios->parts[num].pcir_offset = pcir_offset;
		bios->parts[num].length = pcir_ilen * 0x200;
		if (bios->parts[num].pcir_code_type == 0) {
			int i;
			uint8_t sum = 0;
			uint8_t init_ilen;
			bios_u8(bios, curpos + 2, &init_ilen);
			bios->parts[num].init_length = init_ilen * 0x200;
			for (i = 0; i < bios->parts[num].init_length; i++)
				sum += bios->data[curpos + i];
			bios->parts[num].chksum_pass = (sum == 0);
		}
		curpos += bios->parts[num].length;
	}
	return 0;
}

static unsigned int find_string(struct envy_bios *bios, const uint8_t *str, int len)
{
	int i;
	for (i = 0; i <= (bios->length - len); i++)
		if (!memcmp(bios->data + i, str, len))
			return i;
	return 0;
}

static void parse_bmp_nv03(struct envy_bios *bios) {
	int err = 0;
	err |= bios_u8(bios, bios->bmp_offset + 5, &bios->bmp_ver_major);
	err |= bios_u8(bios, bios->bmp_offset + 6, &bios->bmp_ver_minor);
	err |= bios_u16(bios, bios->bmp_offset + 8, &bios->mode_x86);
	err |= bios_u16(bios, bios->bmp_offset + 0xa, &bios->init_x86);
	err |= bios_u16(bios, bios->bmp_offset + 0xc, &bios->init_script);
	bios->bmp_ver = bios->bmp_ver_major << 8 | bios->bmp_ver_minor;
	if (bios->bmp_ver != 0x01)
		ENVY_BIOS_WARN("NV03 BMP version not 0.1!\n");
	if (!err)
		bios->bmp_length = 0xe;
}

int envy_bios_parse (struct envy_bios *bios) {
	int ret = 0;
	uint16_t vendor, device;
	ret = parse_pcir(bios);
	if (ret)
		return ret;
	if (!bios->partsnum)
		return -EINVAL;
	vendor = bios->parts[0].pcir_vendor;
	device = bios->parts[0].pcir_device;
	switch(vendor) {
	case 0x12d2: /* SGS + nvidia */
		if (device == 0x18 || device == 0x19) {
			bios->type = ENVY_BIOS_TYPE_NV03;
		} else {
			ENVY_BIOS_ERR("Unknown SGS/NVidia pciid %04x\n", device);
			break;
		}
		bios->bmp_offset = find_string(bios, "\xff\x7fNV\0", 5);
		if (!bios->bmp_offset) {
			ENVY_BIOS_ERR("BMP not found\n", device);
			break;
		}
		parse_bmp_nv03(bios);
		break;
	case 0x10de:
		bios->type = ENVY_BIOS_TYPE_NV04;
		bios->bmp_offset = find_string(bios, "\xff\x7f""NV\0", 5);
		bios->bit_offset = find_string(bios, "\xff\xb8""BIT", 5);
		bios->hwsq_offset = find_string(bios, "HWSQ", 4);
		bios_u16(bios, 0x36, &bios->dcb_offset);
		break;
	}
	return 0;
}
