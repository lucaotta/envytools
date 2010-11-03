/*
 *
 * Copyright (C) 2009 Marcin Kościelnicki <koriakin@0x04.net>
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * VA LINUX SYSTEMS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "dis.h"
#include <stdarg.h>

/*
 * Color scheme
 */

char *cnorm = "\x1b[0m";	// lighgray: instruction code and misc stuff
char *cname = "\x1b[0;32m";	// green: instruction name and mods
char *creg0 = "\x1b[0;31m";	// blue: $r registers
char *creg1 = "\x1b[0;35m";	// pink: funny registers
char *cmem = "\x1b[0;36m";	// cyan: memory accesses
char *cnum = "\x1b[0;33m";	// yellow: numbers
char *cunk = "\x1b[1;31m";	// red: unknown stuff
char *cbtarg = "\x1b[0;35m";	// pink: jump labels
char *cctarg = "\x1b[1;37m";	// white: call labels
char *cbctarg = "\x1b[1;35m";	// white: call and jump labels

char *aprint(const char *format, ...) {
	va_list va;
	va_start(va, format);
	size_t sz = vsnprintf(0, 0, format, va);
	va_end(va);
	char *res = malloc(sz + 1);
	va_start(va, format);
	vsnprintf(res, sz + 1, format, va);
	va_end(va);
	return res;
}

void mark(struct disctx *ctx, uint32_t ptr, int m) {
	if (ptr < ctx->codebase || ptr > ctx->codebase + ctx->codesz)
		return;
	ctx->marks[ptr - ctx->codebase] |= m;
}

struct matches *emptymatches() {
	struct matches *res = calloc(sizeof *res, 1);
	return res;
}

struct matches *alwaysmatches(int lpos) {
	struct matches *res = calloc(sizeof *res, 1);
	struct match m = { 0, 0, 0, lpos };
	RNN_ADDARRAY(res->m, m);
	return res;
}

struct matches *catmatches(struct matches *a, struct matches *b) {
	int i;
	for (i = 0; i < b->mnum; i++)
		RNN_ADDARRAY(a->m, b->m[i]);
	free(b->m);
	free(b);
	return a;
}

struct matches *mergematches(struct match a, struct matches *b) {
	struct matches *res = emptymatches();
	int i;
	for (i = 0; i < b->mnum; i++) {
		ull cmask = a.m & b->m[i].m;
		if ((a.a & cmask) == (b->m[i].a & cmask)) {
			struct match nm = b->m[i];
			if (!nm.oplen)
				nm.oplen = a.oplen;
			nm.a |= a.a;
			nm.m |= a.m;
			int j;
			assert (a.nrelocs + nm.nrelocs <= 8);
			for (j = 0; j < a.nrelocs; j++)
				nm.relocs[nm.nrelocs + j] = a.relocs[j];
			nm.nrelocs += a.nrelocs;
			RNN_ADDARRAY(res->m, nm);
		}
	}
	free(b->m);
	free(b);
	return res;
}

struct matches *tabdesc (struct disctx *ctx, struct match m, const struct atom *atoms) {
	if (!atoms->fun) {
		struct matches *res = emptymatches();
		RNN_ADDARRAY(res->m, m);
		return res;
	}
	struct matches *ms = atoms->fun(ctx, 0, 0, atoms->arg, m.lpos);
	if (!ms)
		return 0;
	ms = mergematches(m, ms);
	atoms++;
	if (!atoms->fun)
		return ms;
	struct matches *res = emptymatches();
	int i;
	for (i = 0; i < ms->mnum; i++) {
		struct matches *tmp = tabdesc(ctx, ms->m[i], atoms);
		if (tmp)
			res = catmatches(res, tmp);
	}
	free(ms->m);
	free(ms);
	return res;
}

struct matches *atomtab APROTO {
	const struct insn *tab = v;
	if (!ctx->reverse) {
		int i;
		while ((a[0]&tab->mask) != tab->val || !(tab->ptype & ctx->ptype))
			tab++;
		m[0] |= tab->mask;
		for (i = 0; i < 16; i++)
			if (tab->atoms[i].fun)
				tab->atoms[i].fun (ctx, a, m, tab->atoms[i].arg, 0);
	} else {
		struct matches *res = emptymatches();
		int i;
		for (i = 0; ; i++) {
			if (tab[i].ptype & ctx->ptype) {
				struct match sm = { 0, tab[i].val, tab[i].mask, spos };
				struct matches *subm = tabdesc(ctx, sm, tab[i].atoms); 
				if (subm)
					res = catmatches(res, subm);
			}
			if (!tab[i].mask) break;
		}
		return res;
	}
}

int op8len[] = { 1 };
int op16len[] = { 2 };
int op24len[] = { 3 };
int op32len[] = { 4 };
int op40len[] = { 5 };
int op64len[] = { 8 };
struct matches *atomopl APROTO {
	if (!ctx->reverse) {
		ctx->oplen = *(int*)v;
	} else {
		struct matches *res = alwaysmatches(spos);
		res->m[0].oplen = *(int*)v;
		return res;
	}
}

struct matches *atomendmark APROTO {
	if (!ctx->reverse) {
		ctx->endmark = 1;
	} else {
		struct matches *res = alwaysmatches(spos);
		return res;
	}
}

struct matches *matchid(struct disctx *ctx, int spos, const char *v) {
	if (spos == ctx->line->atomsnum)
		return 0;
	struct expr *e = ctx->line->atoms[spos];
	if (e->type == EXPR_ID && !strcmp(e->str, v))
		return alwaysmatches(spos+1);
	else
		return 0;
}

struct matches *atomname APROTO {
	if (!ctx->reverse) {
		struct expr *expr = makeex(EXPR_ID);
		expr->str = v;
		RNN_ADDARRAY(ctx->atoms, expr);
	} else {
		return matchid(ctx, spos, v);
	}
}

struct matches *atomcmd APROTO {
	if (!ctx->reverse) {
		struct expr *expr = makeex(EXPR_ID);
		expr->str = v;
		expr->special = 1;
		RNN_ADDARRAY(ctx->atoms, expr);
	} else {
		return matchid(ctx, spos, v);
	}
}

struct matches *atomunk APROTO {
	if (ctx->reverse)
		return 0;
	struct expr *expr = makeex(EXPR_ID);
	expr->str = v;
	expr->special = 2;
	RNN_ADDARRAY(ctx->atoms, expr);
}

int setsbf (struct match *res, int pos, int len, ull num) {
	if (!len)
		return 1;
	ull m = ((1ull << len) - 1) << pos;
	ull a = (num << pos) & m;
	if ((a & m & res->m) == (res->a & m & res->m)) {
		res->a |= a;
		res->m |= m;
		return 1;
	} else {
		return 0;
	}
}

int setbfe (struct match *res, const struct bitfield *bf, const struct expr *expr) {
	if (!expr->isimm)
		return 0;
	if (expr->type != EXPR_NUM || bf->pcrel) {
		assert (res->nrelocs != 8);
		res->relocs[res->nrelocs].bf = bf;
		res->relocs[res->nrelocs].expr = expr;
		res->nrelocs++;
		return 1;
	}
	ull num = expr->num1 - bf->addend;
	if (bf->lut) {
		int max = 1 << (bf->sbf[0].len + bf->sbf[1].len);
		int j = 0;
		for (j = 0; j < max; j++)
			if (bf->lut[j] == num)
				break;
		if (j == max)
			return 0;
		num = j;
	}
	num >>= bf->shr;
	setsbf(res, bf->sbf[0].pos, bf->sbf[0].len, num);
	num >>= bf->sbf[0].len;
	setsbf(res, bf->sbf[1].pos, bf->sbf[1].len, num);
	ull mask = ~0ull;
	ull totalsz = bf->shr + bf->sbf[0].len + bf->sbf[1].len;
	if (bf->wrapok && totalsz < 64)
		mask = (1ull << totalsz) - 1;
	return (getbf(bf, &res->a, &res->m, 0) & mask) == (expr->num1 & mask);
}

int setbf (struct match *res, const struct bitfield *bf, ull num) {
	if (bf->pcrel) {
		struct expr *e = makeex(EXPR_NUM);
		e->num1 = num;
		e->isimm = 1;
		return setbfe(res, bf, e);
	} else {
		struct expr e = { .type = EXPR_NUM, .num1 = num, .isimm = 1 };
		return setbfe(res, bf, &e);
	}
}

struct matches *matchimm (struct disctx *ctx, const struct bitfield *bf, int spos) {
	if (spos == ctx->line->atomsnum)
		return 0;
	struct matches *res = alwaysmatches(spos+1);
	if (setbfe(res->m, bf, ctx->line->atoms[spos]))
		return res;
	else {
		free(res->m);
		free(res);
		return 0;
	}
}

struct matches *atomimm APROTO {
	const struct bitfield *bf = v;
	if (ctx->reverse)
		return matchimm(ctx, bf, spos);
	struct expr *expr = makeex(EXPR_NUM);
	expr->num1 = GETBF(bf);
	RNN_ADDARRAY(ctx->atoms, expr);
}

struct matches *atomctarg APROTO {
	const struct bitfield *bf = v;
	if (ctx->reverse)
		return matchimm(ctx, bf, spos);
	struct expr *expr = makeex(EXPR_NUM);
	expr->num1 = GETBF(bf);
	mark(ctx, expr->num1, 2);
	expr->special = 2;
	RNN_ADDARRAY(ctx->atoms, expr);
}

struct matches *atombtarg APROTO {
	const struct bitfield *bf = v;
	if (ctx->reverse)
		return matchimm(ctx, bf, spos);
	struct expr *expr = makeex(EXPR_NUM);
	expr->num1 = GETBF(bf);
	mark(ctx, expr->num1, 1);
	expr->special = 1;
	RNN_ADDARRAY(ctx->atoms, expr);
}

struct matches *atomign APROTO {
	if (ctx->reverse)
		return alwaysmatches(spos);
	const int *n = v;
	(void)BF(n[0], n[1]);
}

int matchreg (struct match *res, const struct reg *reg, const struct expr *expr) {
	if (reg->specials) {
		int i = 0;
		for (i = 0; reg->specials[i].num != -1; i++) {
			switch (reg->specials[i].mode) {
				case SR_NAMED:
					if (expr->type == EXPR_REG && !strcmp(expr->str, reg->specials[i].name))
						return setbf(res, reg->bf, reg->specials[i].num);
					break;
				case SR_ZERO:
					if (expr->type == EXPR_NUM && expr->num1 == 0)
						return setbf(res, reg->bf, reg->specials[i].num);
					break;
				case SR_ONE:
					if (expr->type == EXPR_NUM && expr->num1 == 1)
						return setbf(res, reg->bf, reg->specials[i].num);
					break;
				case SR_DISCARD:
					if (expr->type == EXPR_DISCARD)
						return setbf(res, reg->bf, reg->specials[i].num);
					break;
			}
		}
	}
	if (expr->type != EXPR_REG)
		return 0;
	if (strncmp(expr->str, reg->name, strlen(reg->name)))
		return 0;
	const char *str = expr->str + strlen(reg->name);
	char *end;
	ull num = strtoull(str, &end, 10);
	if (!reg->hilo) {
		if (strcmp(end, (reg->suffix?reg->suffix:"")))
			return 0;
	} else {
		if (strlen(end) != 1)
			return 0;
		if (*end != 'h' && *end != 'l')
			return 0;
		num <<= 1;
		if (*end == 'h')
			num |= 1;
	}
	if (reg->bf)
		return setbf(res, reg->bf, num);
	else
		return !num;
}

int matchshreg (struct match *res, const struct reg *reg, const struct expr *expr, int shl) {
	ull sh = 0;
	while (1) {
		if (expr->type == EXPR_SHL && expr->expr2->type == EXPR_NUM) {
			sh += expr->expr2->num1;
			expr = expr->expr1;
		} else if (expr->type == EXPR_MUL && expr->expr2->type == EXPR_NUM) {
			ull num = expr->expr2->num1;
			if (!num || (num & num-1))
				return 0;
			while (num != 1)
				num >>= 1, sh++;
			expr = expr->expr1;
		} else if (expr->type == EXPR_MUL && expr->expr1->type == EXPR_NUM) {
			ull num = expr->expr1->num1;
			if (!num || (num & num-1))
				return 0;
			while (num != 1)
				num >>= 1, sh++;
			expr = expr->expr2;
		} else {
			break;
		}
	}
	if (sh != shl)
		return 0;
	return matchreg(res, reg, expr);
}

// print reg if non-0, ignore otherwise. return 1 if printed anything.
static struct expr *printreg (struct disctx *ctx, ull *a, ull *m, const struct reg *reg) {
	struct expr *expr;
	ull num = 0;
	if (reg->bf)
		num = GETBF(reg->bf);
	if (reg->specials) {
		int i;
		for (i = 0; reg->specials[i].num != -1; i++) {
			if (num == reg->specials[i].num) {
				switch (reg->specials[i].mode) {
					case SR_NAMED:
						expr = makeex(EXPR_REG);
						expr->str = reg->specials[i].name;
						expr->special = 1;
						return expr;
					case SR_ZERO:
						return 0;
					case SR_ONE:
						expr = makeex(EXPR_NUM);
						expr->num1 = 1;
						return expr;
					case SR_DISCARD:
						expr = makeex(EXPR_DISCARD);
						return expr;
				}
			}
		}
	}
	expr = makeex(EXPR_REG);
	const char *suf = "";
	if (reg->suffix)
		suf = reg->suffix;
	if (reg->hilo) {
		if (num & 1)
			suf = "h";
		else
			suf = "l";
		num >>= 1;
	}
	expr->special = reg->cool;
	if (reg->always_special)
		expr->special = 2;
	if (reg->bf)
		expr->str = aprint("%s%lld%s", reg->name, num, suf);
	else
		expr->str = aprint("%s%s", reg->name, suf);
	return expr;
}

struct matches *atomreg APROTO {
	const struct reg *reg = v;
	if (ctx->reverse) {
		if (spos == ctx->line->atomsnum)
			return 0;
		struct matches *res = alwaysmatches(spos+1);
		if (matchreg(res->m, reg, ctx->line->atoms[spos]))
			return res;
		else {
			free(res->m);
			free(res);
			return 0;
		}
	}
	struct expr *expr = printreg(ctx, a, m, reg);
	if (!expr) expr = makeex(EXPR_NUM);
	RNN_ADDARRAY(ctx->atoms, expr);
}

int addexpr (const struct expr **iex, const struct expr *expr, int flip) {
	if (flip) {
		if (!*iex)
			*iex = makeunex(EXPR_NEG, expr);
		else
			*iex = makebinex(EXPR_SUB, *iex, expr);
	} else {
		if (!*iex)
			*iex = expr;
		else
			*iex = makebinex(EXPR_ADD, *iex, expr);
	}
	return 1;
}

int matchmemaddr(const struct expr **iex, const struct expr **niex1, const struct expr **niex2, const struct expr **piex, const struct expr *expr, int flip) {
	if (expr->type == EXPR_PIADD || expr->type == EXPR_PISUB) {
		if (flip || !expr->expr2->isimm)
			return 0;
		return matchmemaddr(iex, niex1, niex2, piex, expr->expr1, 0) && addexpr(piex, expr->expr2, expr->type == EXPR_PISUB);
	}
	if (expr->isimm)
		return addexpr(iex, expr, flip);
	if (expr->type == EXPR_ADD)
		return matchmemaddr(iex, niex1, niex2, piex, expr->expr1, flip) && matchmemaddr(iex, niex1, niex2, piex, expr->expr2, flip);
	if (expr->type == EXPR_SUB)
		return matchmemaddr(iex, niex1, niex2, piex, expr->expr1, flip) && matchmemaddr(iex, niex1, niex2, piex, expr->expr2, !flip);
	if (expr->type == EXPR_NEG)
		return matchmemaddr(iex, niex1, niex2, piex, expr->expr1, !flip);
	if (flip)
		return 0;
	if (niex1 && !*niex1)
		*niex1 = expr;
	else if (niex2 && !*niex2)
		*niex2 = expr;
	else
		return 0;

}

struct matches *atommem APROTO {
	const struct mem *mem = v;
	if (!ctx->reverse) {
		struct expr *expr = 0;
		if (mem->reg)
			expr = printreg(ctx, a, m, mem->reg);
		if (mem->imm) {
			ull imm = GETBF(mem->imm);
			if (mem->postincr) {
				struct expr *sexpr = makeex(EXPR_NUM);
				if (imm & 1ull << 63) {
					sexpr->num1 = -imm;
					expr = makebinex(EXPR_PISUB, expr, sexpr);
				} else {
					sexpr->num1 = imm;
					expr = makebinex(EXPR_PIADD, expr, sexpr);
				}
			} else if (imm) {
				struct expr *sexpr = makeex(EXPR_NUM);
				if (expr) {
					if (imm & 1ull << 63) {
						sexpr->num1 = -imm;
						expr = makebinex(EXPR_SUB, expr, sexpr);
					} else {
						sexpr->num1 = imm;
						expr = makebinex(EXPR_ADD, expr, sexpr);
					}
				} else {
					sexpr->num1 = imm;
					expr = sexpr;
				}
			}
		}
		if (mem->reg2) {
			struct expr *sexpr = printreg(ctx, a, m, mem->reg2);
			if (mem->reg2shr) {
				struct expr *ssexpr = makeex(EXPR_NUM);
				ssexpr->num1 = 1ull << mem->reg2shr;
				sexpr = makebinex(EXPR_MUL, sexpr, ssexpr);
			}
			if (expr)
				expr = makebinex(EXPR_ADD, expr, sexpr);
			else
				expr = sexpr;
		}
		if (!expr) expr = makeex(EXPR_NUM);
		if (mem->name) {
			struct expr *nex = makeex(EXPR_MEM);
			nex->expr1 = expr;
			nex->str = mem->name;
			if (mem->idx)
				nex->str = aprint("%s%lld", nex->str, GETBF(mem->idx));
			expr = nex;
		}
		RNN_ADDARRAY(ctx->atoms, expr);
	} else {
		if (spos == ctx->line->atomsnum)
			return 0;
		struct match res = { 0, .lpos = spos+1 };
		const struct expr *expr = ctx->line->atoms[spos];
		if (expr->type == EXPR_MEM && !mem->name)
			return 0;
		if (expr->type != EXPR_MEM && mem->name)
			return 0;
		if (expr->type == EXPR_MEM) {
			if (strncmp(expr->str, mem->name, strlen(mem->name)))
				return 0;
			if (mem->idx) {
				const char *str = expr->str + strlen(mem->name);
				if (!*str)
					return 0;
				char *end;
				ull num = strtoull(str, &end, 10);
				if (*end)
					return 0;
				if (!setbf(&res, mem->idx, num))
					return 0;
			} else {
				if (strlen(expr->str) != strlen(mem->name))
					return 0;
			}
			expr = expr->expr1;
		}
		const struct expr *iex = 0;
		const struct expr *niex1 = 0;
		const struct expr *niex2 = 0;
		const struct expr *piex = 0;
		matchmemaddr(&iex, &niex1, &niex2, &piex, expr, 0);
		if (mem->imm) {
			if (mem->postincr) {
				if (!piex || iex)
					return 0;
				if (!setbfe(&res, mem->imm, piex))
					return 0;
			} else {
				if (piex)
					return 0;
				if (iex) {
					if (!setbfe(&res, mem->imm, iex))
						return 0;
				} else {
					if (!setbf(&res, mem->imm, 0))
						return 0;
				}
			}
		} else {
			if (iex)
				return 0;
		}
		const static struct expr e0 = { .type = EXPR_NUM, .isimm = 1 };
		if (mem->reg && mem->reg2) {
			if (!niex1)
				niex1 = &e0;
			if (!niex2)
				niex2 = &e0;
			struct match sres = res;
			if (!matchreg(&res, mem->reg, niex1) || !matchshreg(&res, mem->reg2, niex2, mem->reg2shr)) {
				res = sres;
				if (!matchreg(&res, mem->reg, niex2) || !matchshreg(&res, mem->reg2, niex1, mem->reg2shr))
					return 0;
			}
		} else if (mem->reg) {
			if (niex2)
				return 0;
			if (!niex1)
				niex1 = &e0;
			if (!matchreg(&res, mem->reg, niex1))
				return 0;
		} else if (mem->reg2) {
			if (niex2)
				return 0;
			if (!niex1)
				niex1 = &e0;
			if (!matchshreg(&res, mem->reg2, niex1, mem->reg2shr))
				return 0;
		} else {
			if (niex1 || niex2)
				return 0;
		}
		struct matches *rres = emptymatches();
		RNN_ADDARRAY(rres->m, res);
		return rres;
	}
}

struct matches *atomvec APROTO {
	const struct vec *vec = v;
	if (!ctx->reverse) {
		struct expr *expr = makeex(EXPR_VEC);
		ull base = GETBF(vec->bf);
		ull cnt = GETBF(vec->cnt);
		ull mask = -1ull;
		if (vec->mask)
			mask = GETBF(vec->mask);
		int k = 0, i;
		for (i = 0; i < cnt; i++) {
			if (mask & 1ull<<i) {
				struct expr *sexpr = makeex(EXPR_REG);
				sexpr->special = vec->cool;
				sexpr->str = aprint("%s%lld", vec->name,  base + k++);
				RNN_ADDARRAY(expr->vexprs, sexpr);
			} else {
				struct expr *sexpr = makeex(EXPR_DISCARD);
				RNN_ADDARRAY(expr->vexprs, sexpr);
			}
		}
		RNN_ADDARRAY(ctx->atoms, expr);
	} else {
		if (spos == ctx->line->atomsnum)
			return 0;
		struct match res = { 0, .lpos = spos+1 };
		const struct expr *expr = ctx->line->atoms[spos];
		if (expr->type != EXPR_VEC)
			return 0;
		ull start = 0, cnt = expr->vexprsnum, mask = 0, cur = 0;
		int i;
		for (i = 0; i < cnt; i++) {
			const struct expr *e = expr->vexprs[i];
			if (e->type == EXPR_DISCARD) {
			} else if (e->type == EXPR_REG) {
				if (strncmp(e->str, vec->name, strlen(vec->name)))
					return 0;
				char *end;
				ull num = strtoull(e->str + strlen(vec->name), &end, 10);
				if (*end)
					return 0;
				if (mask) {
					if (num != cur)
						return 0;
					cur++;
				} else {
					start = num;
					cur = num + 1;
				}
				mask |= 1ull << i;
			} else {
				return 0;
			}
		}
		if (!setbf(&res, vec->bf, start))
			return 0;
		if (!setbf(&res, vec->cnt, cnt))
			return 0;
		if (vec->mask) {
			if (!setbf(&res, vec->mask, mask))
				return 0;
		} else {
			if (mask != (1ull << cnt) - 1)
			       return 0;	
		}
		struct matches *rres = emptymatches();
		RNN_ADDARRAY(rres->m, res);
		return rres;
	}
}

struct matches *atombf APROTO {
	const struct bitfield *bf = v;
	if (!ctx->reverse) {
		struct expr *expr = makeex(EXPR_BITFIELD);
		expr->num1 = GETBF(&bf[0]);
		expr->num2 = GETBF(&bf[1]);
		RNN_ADDARRAY(ctx->atoms, expr);
	} else {
		if (spos == ctx->line->atomsnum)
			return 0;
		struct match res = { 0, .lpos = spos+1 };
		const struct expr *expr = ctx->line->atoms[spos];
		if (expr->type != EXPR_BITFIELD)
			return 0;
		ull a = expr->num1;
		ull b = expr->num2 - a;
		if (!setbf(&res, &bf[0], a))
			return 0;
		if (!setbf(&res, &bf[1], b))
			return 0;
		struct matches *rres = emptymatches();
		RNN_ADDARRAY(rres->m, res);
		return rres;
	}
}

ull getbf(const struct bitfield *bf, ull *a, ull *m, struct disctx *ctx) {
	ull res = 0;
	int pos = bf->shr;
	int i;
	for (i = 0; i < sizeof(bf->sbf) / sizeof(bf->sbf[0]); i++) {
		res |= BF(bf->sbf[i].pos, bf->sbf[i].len) << pos;
		pos += bf->sbf[i].len;
	}
	switch (bf->mode) {
		case BF_UNSIGNED:
			break;
		case BF_SIGNED:
			if (res & 1ull << (pos - 1))
				res -= 1ull << pos;
			break;
		case BF_SLIGHTLY_SIGNED:
			if (res & 1ull << (pos - 1) && res & 1ull << (pos - 2))
				res -= 1ull << pos;
			break;
		case BF_ULTRASIGNED:
			res -= 1ull << pos;
			break;
		case BF_LUT:
			res = bf->lut[res];
			break;
	}
	if (bf->pcrel) {
		// <3 xtensa.
		res += (ctx->pos + bf->pospreadd) & -(1ull << bf->shr);
	}
	res += bf->addend;
	return res;
}

uint32_t readle32 (uint8_t *p) {
	return p[0] | p[1] << 8 | p[2] << 16 | p[3] << 24;
}

uint16_t readle16 (uint8_t *p) {
	return p[0] | p[1] << 8;
}

/*
 * Disassembler driver
 *
 * You pass a block of memory to this function, disassembly goes out to given
 * FILE*.
 */

void envydis (struct disisa *isa, FILE *out, uint8_t *code, uint32_t start, int num, int ptype, int quiet, struct label *labels, int labelsnum)
{
	struct disctx c = { 0 };
	struct disctx *ctx = &c;
	int cur = 0, i, j;
	ctx->code8 = code;
	ctx->marks = calloc((num + isa->posunit - 1) / isa->posunit, sizeof *ctx->marks);
	ctx->codebase = start;
	ctx->codesz = num;
	ctx->ptype = ptype;
	ctx->isa = isa;
	if (labels) {
		for (i = 0; i < labelsnum; i++)
			mark(ctx, labels[i].val, 4);
		int done;
		do {
			done = 1;
			cur = 0;
			int active = 0;
			while (cur < num) {
				if (!active && (ctx->marks[cur / isa->posunit] & 7) && !(ctx->marks[cur / isa->posunit] & 8)) {
					done = 0;
					active = 1;
					ctx->marks[cur / isa->posunit] |= 8;
				}
				if (active) {
					ull a = 0, m = 0;
					for (i = 0; i < 8 && cur + i < num; i++) {
						a |= (ull)code[cur + i] << i*8;
					}
					ctx->oplen = 0;
					ctx->pos = cur / isa->posunit + start;
					ctx->endmark = 0;
					atomtab (ctx, &a, &m, isa->troot, 0);
					if (ctx->oplen && !ctx->endmark)
						cur += ctx->oplen;
					else
						active = 0;
				} else {
					cur++;
				}
			}
		} while (!done);
	} else {
		while (cur < num) {
			ull a = 0, m = 0;
			for (i = 0; i < 8 && cur + i < num; i++) {
				a |= (ull)code[cur + i] << i*8;
			}
			ctx->oplen = 0;
			ctx->pos = cur / isa->posunit + start;
			atomtab (ctx, &a, &m, isa->troot, 0);
			if (ctx->oplen)
				cur += ctx->oplen;
			else
				cur++;
		}
	}
	cur = 0;
	int active = 0;
	int skip = 0;
	while (cur < num) {
		if (!active && ctx->marks[cur / isa->posunit])
			active = 1;
		if (!active && labels) {
			cur++;
			skip++;
			continue;
		}
		if (skip) {
			printf("%s[%x bytes skipped]\n", cnorm, skip);
			skip = 0;
		}
		ull a = 0, m = 0;
		for (i = 0; i < 8 && cur + i < num; i++) {
			a |= (ull)code[cur + i] << i*8;
		}
		ctx->oplen = 0;
		ctx->pos = cur / isa->posunit + start;
		ctx->atomsnum = 0;
		ctx->endmark = 0;
		atomtab (ctx, &a, &m, isa->troot, 0);

		if (ctx->endmark)
			active = 0;

		if (ctx->marks[cur / isa->posunit] & 2)
			fprintf (out, "\n");
		switch (ctx->marks[cur / isa->posunit] & 3) {
			case 0:
				if (!quiet)
					fprintf (out, "%s%08x:%s", cnorm, cur / isa->posunit + start, cnorm);
				break;
			case 1:
				fprintf (out, "%s%08x:%s", cbtarg, cur / isa->posunit + start, cnorm);
				break;
			case 2:
				fprintf (out, "%s%08x:%s", cctarg, cur / isa->posunit + start, cnorm);
				break;
			case 3:
				fprintf (out, "%s%08x:%s", cbctarg, cur / isa->posunit + start, cnorm);
				break;
		}

		if (!quiet) {
			for (i = 0; i < isa->maxoplen; i += isa->opunit) {
				fprintf (out, " ");
				for (j = isa->opunit - 1; j >= 0; j--)
					if (i+j && i+j >= ctx->oplen)
						fprintf (out, "  ");
					else if (cur+i+j >= num)
						fprintf (out, "%s??", cunk);
					else
						fprintf (out, "%s%02x", cnorm, code[cur + i + j]);
			}
			fprintf (out, "  ");

			if (ctx->marks[cur / isa->posunit] & 2)
				fprintf (out, "%sC", cctarg);
			else
				fprintf (out, " ");
			if (ctx->marks[cur / isa->posunit] & 1)
				fprintf (out, "%sB", cbtarg);
			else
				fprintf (out, " ");
		} else if (quiet == 1) {
			if (ctx->marks[cur / isa->posunit])
				fprintf (out, "\n");
		}

		for (i = 0; i < ctx->atomsnum; i++) {
			if (i || !quiet)
				fprintf (out, " ");
			printexpr(out, ctx->atoms[i], 0);
		}

		if (ctx->oplen) {
			a &= ~m;
			if (ctx->oplen < 8)
				a &= (1ull << ctx->oplen * 8) - 1;
			if (a) {
				fprintf (out, " %s[unknown:", cunk);
				for (i = 0; i < ctx->oplen || i == 0; i += isa->opunit) {
					fprintf (out, " ");
					for (j = isa->opunit - 1; j >= 0; j--)
						if (cur+i+j >= num)
							fprintf (out, "??");
						else
							fprintf (out, "%02llx", (a >> (i + j) * 8) & 0xff);
				}
				fprintf (out, "]");
			}
			if (cur + ctx->oplen > num) {
				fprintf (out, " %s[incomplete]%s", cunk, cnorm);
			}
			cur += ctx->oplen;
		} else {
			fprintf (out, " %s[unknown op length]%s", cunk, cnorm);
			cur++;
		}
		fprintf (out, "%s\n", cnorm);
	}
	free(ctx->marks);
}
