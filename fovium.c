#include <stdio.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <SDL/SDL_endian.h>

#include "sdl.h"

void go(void);
void use_normal(void);
void use_swapped(void);

typedef uint32_t Cell;
typedef void (*Instruction)(void);

Cell *BP;  // Base Pointer
Cell *IP;  // Instruction Pointer
Cell IL;   // Instruction Latch (cell at IP)
Cell *DS;  // Data Stack pointer
Cell *RS;  // Return Stack pointer
Cell FS;   // Flag Stack (circular 32-entry stack)
union {
	Cell *w;
	uint16_t *h;
	uint8_t *b;
} A;       // Address register


// *********************************************************************
// Helper Functions

// Data Stack
static inline void PUSH(Cell c) { *(--DS) = c; }
static inline Cell POP(void) { return *DS++; }
static inline void DROPN(Cell n) { DS += n; }
#define T (DS[0])
#define N (DS[1])
#define DROP DROPN(1)

// Return Stack
static inline void RPUSH(Cell c) { *(--RS) = c; }
static inline Cell RPOP(void) { return *RS++; }

// Flag Stack
static inline Cell FLAG(void) { return (FS>>31) & 1; }
static inline void FDROP(void) { FS = (FS<<1) | ((FS>>31)& 1); }
static inline void FPUSH(Cell c) { FS = (FS>>1) | ((c!=0)<<31); }
static inline Cell FPOP(void) { Cell f = FLAG(); FDROP(); return f; }

// Next Word (lits, branch addresses, etc.)
static inline Cell NEXT(void) { return *IP++; }
static inline Cell NEXT_SWAPPED(void) { return SDL_Swap32(*IP++); }

// Addresses
static inline Cell MASK(Cell c) { return c & 0xfffff; }
static inline Cell OFFSET(Cell *p) { return (Cell)((void *)p - (void *)BP); }
static inline Cell *PTR(Cell c) { return (Cell *)(MASK(c) + (void *)BP); }
static inline uint16_t *HPTR(Cell c) { return (uint16_t *)PTR(c); }
static inline uint8_t *BPTR(Cell c) { return (uint8_t *)PTR(c); }


// *********************************************************************
// Initialization

int pagesz, pmask;
int img_len;
Cell *ds_base;
Cell *rs_base;

// save needs access to argv
char **gargv;

int
main(int argc, char **argv)
{
	FILE *fp;

	if(argc != 2) {
		puts("Usage: fovium <image-file>");
		return 0;
	}

	gargv = argv;

	init_gfx();

	fp = fopen(argv[1], "rb");
	if(!fp) {
		printf("Error: can't open \"%s\".\n", argv[1]);
		return 1;
	}

	pagesz = getpagesize();
	pmask = pagesz-1;

	fseek(fp, 0, SEEK_END);
	img_len = ftell(fp);
	if(img_len > 1024*1024) {
		printf("Error: We currently only handle images up to 1MB in size.\n");
		return 1;
	}
	BP = (Cell*) malloc(1024*1024); if(!BP) return 1;
	fseek(fp, 0, SEEK_SET);
	fread(BP, 1, img_len, fp);
	fclose(fp);

	IP = BP;
	IL = *(IP++);
	if((IL & 0x3f000000) == 0x0f000000) {
		IL = SDL_Swap32(IL);
		use_swapped();
	} else if((IL & 0x0000003f) != 0x0000000f) {
		fprintf(stderr, "error: images should start with a branch.\n");
		return 1;
	}

	ds_base = (Cell*) malloc(4096); if(!ds_base) return 1;
	rs_base = (Cell*) malloc(4096); if(!rs_base) return 1;
	DS = ds_base + 4096/sizeof(Cell);
	RS = rs_base + 4096/sizeof(Cell);

	FS = 0;

	init_gfx();

	go();

	return 0;  // Never happens -- just to shut up the compiler
}


// *********************************************************************
// Syscalls

void nop(void) {}
void sc_exit(void)
{
	Cell r = T;
	free(rs_base);
	free(ds_base);
	free(BP);
	exit(r);
}

void emit(void) {
	int c = POP();
	sdl_emit(c);
#ifdef EMIT_TO_TERM
	putchar(c);
	fflush(stdout);
#endif
}

void wait_event(void) {
	int microsec = POP();
	int *rets = sdl_key(microsec); // pushes three things
	if(!rets) {
		FPUSH(0);
		return;
	}

	FPUSH(1);
	PUSH(rets[2]);
	PUSH(rets[1]);
	PUSH(rets[0]);
}

void term_color(void) {
	int c = POP();
	font_color(c);
#ifdef COLOR_TO_TERM
	printf("\e[3%cm", '0' + (c % 10));
#endif
}
void term_move(void) {
	int pos;
	pos = POP();
	sdl_term_move(pos);
}

void save(void) {
	static int base_len = 0;
	static char *name = 0;
	static int num = 0;
	int len;
	FILE *fd;
	uint8_t *addr;
	Cell caddr;

	if(name == 0) {
		base_len = strlen(gargv[1]);
		name = (char*) malloc(base_len + 10);

		// if the name of the image already has a number at the end, convert it to
		// int, add one, and ignore the characters at the end
		if(gargv[1][base_len-1] <= '9' && gargv[1][base_len-1] <= '9') {
			num = atoi(&(gargv[1][base_len-5])) + 1;
			base_len -= 6;
		}

		memcpy(name, gargv[1], base_len);
	} else {
		++num;
	}

	sprintf(&(name[base_len]), "-%05d", num);

	len = POP();
	caddr = MASK(POP());
	// don't allow save region to go off the end of the memory image
	if(caddr + len > 1024*1024) {
		len = 1024*1024 - caddr;
	}
	addr = BPTR(caddr);

	fd = fopen(name, "wb");
	fwrite(addr, 1, len, fd);
	fclose(fd);
}





	

Instruction syscalls[64] = {
	sc_exit, save, nop, nop, nop, nop, nop, nop,
	nop, nop, nop, nop, nop, nop, nop, nop,
	emit, wait_event, term_color, term_move, nop, nop, nop, nop,
	nop, nop, nop, nop, nop, nop, nop, nop,
	nop, nop, nop, nop, nop, nop, nop, nop,
	nop, nop, nop, nop, nop, nop, nop, nop,
	nop, nop, nop, nop, nop, nop, nop, nop,
	nop, nop, nop, nop, nop, nop, nop, nop
};


// *********************************************************************
// VM Instructions

static inline void nexti(void) { IL = NEXT(); }
static inline void semi(void) { IP = PTR(RPOP()); IL = NEXT(); }
static inline void branch(void) { IP = PTR(IL); IL = NEXT(); }
void call(void) { RPUSH(OFFSET(IP)); IP = PTR(IL); IL = NEXT(); }
void lit(void) { PUSH(NEXT()); }

static inline void nexti_swapped(void) { IL = NEXT_SWAPPED(); }
static inline void semi_swapped(void) { IP = PTR(RPOP()); IL = NEXT_SWAPPED(); }
static inline void branch_swapped(void) { IP = PTR(IL); IL = NEXT_SWAPPED(); }
void call_swapped(void) { RPUSH(OFFSET(IP)); IP = PTR(IL); IL = NEXT_SWAPPED(); }
void lit_swapped(void) { PUSH(NEXT_SWAPPED()); }

void dup_(void) { PUSH(T); }
void drop(void) { DROP; }
void swap(void) { Cell n = N; N = T; T = n; }
void over(void) { PUSH(N); }
void nip(void) { N = T; DROP; }
void rot(void) { Cell t = T; T = DS[2]; DS[2] = N; N = t; }

void to_r(void) { RPUSH(POP()); }
void dup_to_r(void) { RPUSH(T); }
void r_fetch(void) { PUSH(*RS); }
void r_from(void) { PUSH(RPOP()); }
void rdrop(void) { RS++; }

void qbranch(void) { if(FPOP()) branch(); else IL = NEXT(); }
void qbranch_swapped(void) { if(FPOP()) branch_swapped(); else IL = NEXT_SWAPPED(); }
void zbranch(void) { if(!FPOP()) branch(); else IL = NEXT(); }
void zbranch_swapped(void) { if(!FPOP()) branch_swapped(); else IL = NEXT_SWAPPED(); }
void qsemi_swapped(void) { if(FPOP()) semi_swapped(); }
void zsemi_swapped(void) { if(!FPOP()) semi_swapped(); }
void tsemi_swapped(void) { if(FLAG()) semi_swapped(); else FDROP(); }
void fsemi_swapped(void) { if(!FLAG()) semi_swapped(); else FDROP(); }
void qsemi(void) { if(FPOP()) semi(); }
void zsemi(void) { if(!FPOP()) semi(); }
void tsemi(void) { if(FLAG()) semi(); else FDROP(); }
void fsemi(void) { if(!FLAG()) semi(); else FDROP(); }
void question(void) { FPUSH(T!=0); }
void zequals(void) { FPUSH(T==0); DROP; }
void equals(void) { FPUSH(N==T); DROPN(2); }
void less(void) { FPUSH(N<T); DROPN(2); }
void andfl(void) { FPUSH(FPOP() & FPOP()); }
void orfl(void) { FPUSH(FPOP() | FPOP()); }
void xorfl(void) { FPUSH(FPOP() ^ FPOP()); }
void notfl(void) { FPUSH(1^FPOP()); }

void and(void) { N &= T; DROP; }
void or(void) { N |= T; DROP; }
void xor(void) { N ^= T; DROP; }
void not(void) { T = ~T; }

void rshift(void) { N >>= T; DROP; }
void srshift(void) { N = (Cell)((int32_t)N >> T); DROP; }
void lshift(void) { N <<= T; DROP; }
void lrot(void) { N = (N << T) | (N >> (32-T)); DROP; }

void plus(void) { N += T; DROP; }
void minus(void) { N -= T; DROP; }
void star(void) { N *= T; DROP; }
void slash(void) { N /= T; DROP; }
void slash_mod(void) { Cell n = N; N %= T; T = n / T; }

void one_plus(void) { T++; }
void one_minus(void) { T--; }
void four_plus(void) { T += 4; }
void four_minus(void) { T -= 4; }
void four_star(void) { T <<= 2; }
void eight_plus(void) { T += 8; }

void to_a(void) { A.w = PTR(T); DROP; }
void a(void) { PUSH(OFFSET(A.w)); }
void fetch_a(void) { PUSH(*A.w); }
void fetch_a_swapped(void) { PUSH(SDL_Swap32(*A.w)); }
void store_a(void) { *A.w = T; DROP; }
void store_a_swapped(void) { *A.w = SDL_Swap32(T); DROP; }
void plus_fetch(void) { A.w++; PUSH(*A.w); }
void plus_fetch_swapped(void) { A.w++; PUSH(SDL_Swap32(*A.w)); }
void b_plus_fetch(void) { A.b++; PUSH(*A.b); }
void plus_store(void) { A.w++; *A.w = T; DROP; }
void plus_store_swapped(void) { A.w++; *A.w = SDL_Swap32(T); DROP; }
void b_plus_store(void) { A.b++; *A.b = T; DROP; }

void fetch(void) { T = *PTR(T); }
void fetch_swapped(void) { T = SDL_Swap32(*PTR(T)); }
void store(void) { *PTR(T) = N; DROPN(2); }
void store_swapped(void) { *PTR(T) = SDL_Swap32(N); DROPN(2); }
void h_fetch(void) { T = *HPTR(T); }
void h_fetch_swapped(void) { T = SDL_Swap16(*HPTR(T)); }
void h_store(void) { *HPTR(T) = N; DROPN(2); }
void h_store_swapped(void) { *HPTR(T) = SDL_Swap16(N); DROPN(2); }
void b_fetch(void) { T = *BPTR(T); }
void b_store(void) { *BPTR(T) = N; DROPN(2); }

void syscall_(void)
{
	Instruction sc = syscalls[T];
	DROP;
	sc();
}


// *********************************************************************
// Instruction table and interpreter

Instruction instrs[64] = {
	nexti, dup_, call, lit, drop, swap, over, nip,
	rot, to_r, dup_to_r, r_fetch, r_from, rdrop, semi, branch,
	qbranch, zbranch, qsemi, zsemi, tsemi, fsemi, question, zequals,
	equals, less, andfl, orfl, xorfl, notfl, and, or,
	xor, not, rshift, srshift, lshift, lrot, plus, minus,
	star, slash, slash_mod, one_plus, one_minus, four_plus, four_minus, four_star,
	eight_plus, to_a, a, fetch_a, store_a, plus_fetch, b_plus_fetch, plus_store,
	b_plus_store, fetch, store, h_fetch, h_store, b_fetch, b_store, syscall_
};

void
go(void)
{
	Instruction i;
	while(1) {
		i = instrs[IL&63]; IL >>= 6;
		i();
	}
}

void
use_swapped(void)
{
	instrs[0] = nexti_swapped;
	instrs[14] = semi_swapped;
	instrs[18] = qsemi_swapped;
	instrs[19] = zsemi_swapped;
	instrs[20] = tsemi_swapped;
	instrs[21] = fsemi_swapped;
	instrs[15] = branch_swapped;
	instrs[16] = qbranch_swapped;
	instrs[17] = zbranch_swapped;
	instrs[2] = call_swapped;
	instrs[3] = lit_swapped;
	instrs[51] = fetch_a_swapped;
	instrs[52] = store_a_swapped;
	instrs[53] = plus_fetch_swapped;
	instrs[55] = plus_store_swapped;
	instrs[57] = fetch_swapped;
	instrs[58] = store_swapped;
	instrs[59] = h_fetch_swapped;
	instrs[60] = h_store_swapped;
}

void
use_normal(void)
{
	instrs[0] = nexti;
	instrs[14] = semi;
	instrs[18] = qsemi;
	instrs[19] = zsemi;
	instrs[20] = tsemi;
	instrs[21] = fsemi;
	instrs[15] = branch;
	instrs[16] = qbranch;
	instrs[17] = zbranch;
	instrs[2] = call;
	instrs[3] = lit;
	instrs[51] = fetch_a;
	instrs[52] = store_a;
	instrs[53] = plus_fetch;
	instrs[55] = plus_store;
	instrs[57] = fetch;
	instrs[58] = store;
	instrs[59] = h_fetch;
	instrs[60] = h_store;
}
