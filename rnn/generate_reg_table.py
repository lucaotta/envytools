#!/usr/bin/python

import sys
from lxml import etree as ET
import argparse
import os.path

MAX_CARD_TYPE = 0xFFFF
TEMPLATE = '{0x%x, 0x%x, 0x%x, %s},'

def find_address(elem):
	address = 0
	variant = ''
	while elem != None:
		address += int(elem.attrib.get('offset', '0'), 16)
		new_variant = elem.attrib.get('variants', '')
		if new_variant != '' and variant != '':
			pass
		elif variant == '':
			variant = new_variant
		elem = elem.getparent()
	return (address, variant)

def parse_variant(variant):
	if '-' in variant:
		tmp = map(lambda x: x[2:], variant.split('-'))
		res = []
		res.append(int(tmp[0], 16) if tmp[0] != '' else 0)
		res.append(int(tmp[1], 16) + 1 if tmp[1] != '' else MAX_CARD_TYPE)
		# include end into the range
		return res
	if ':' in variant:
		tmp = map(lambda x: x[2:], variant.split(':'))
		res = []
		res.append(int(tmp[0], 16) if tmp[0] != '' else 0)
		res.append(int(tmp[1], 16) if tmp[1] != '' else MAX_CARD_TYPE)
		return res
	return [int(variant[2:], 16), int(variant[2:], 16) + 1]

def range_from_variant(var_list):
	# variants may be space separated ranges
	return map(parse_variant, var_list.split())

def parse_file(xml_file):
	tree = ET.parse(xml_file)
	root = tree.getroot();

	reg_list = []
	try:
		default_namespace = root.nsmap[None]
	except KeyError:
		sys.stderr.write("Missing namespace, ignoring file: %s\n" % xml_file)
		return []

	for i in root.iter(tag='{%s}reg32' % (default_namespace)):
		if 'status' in i.attrib['name'].lower():
			(address, variants) = find_address(i)
			if variants:
				var = range_from_variant(variants)
				reg_list.extend(map(lambda l: TEMPLATE % (l[0], l[1], address, 'false'), var))
	return reg_list

def generate_header_file(output):
	output.write('struct StatusRegister register_table[] = {\n')
	output.writelines('\t%s\n' % (i) for i in reg_list)
	output.write('};\n')

if __name__ == '__main__':
	parser = argparse.ArgumentParser()
	parser.add_argument('-o', '--output', nargs=1, default=[sys.stdout], type=argparse.FileType('w'))
	parser.add_argument('input_files', nargs='*', default=[sys.stdin])

	args = parser.parse_args()
	reg_list = []
	
	for xml in args.input_files:
		reg_list.extend(parse_file(xml))

	for output in args.output:
		generate_header_file(output)

