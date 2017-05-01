# Register Assignments
	.set BP, 2  #Base Pointer
	.set T, 14  #Top of stack
	.set IP,15  #Instruction Pointer
	.set IL,16  #Instruction Latch (cell at IP)
	.set IT,17  #Instruction Table (VM primitives)
	.set DS,18  #Data Stack pointer (grows up)
	.set RS,19  #Return Stack pointer (grows up)
	.set FS,20  #Flag Stack (circular 32-entry stack)
	.set A, 21  #Address register


# Macros

	#Load Word Immediate
	.macro lwi reg,value
	lis \reg,\value@ha
	addi \reg,\reg,\value@l
	.endm

	.macro prolog
	mflr r0
	stwu r0,-16(r1)  #leave room for some temp storage
	.endm

	.macro epilog
	lwz r0,0(r1)
	addi r1,r1,16
	mtlr r0
	blr
	.endm


# PPC Linux Constants
	#file descriptors
	.set stdin, 0
	.set stdout, 1
	.set stderr, 2

	#flags for open/creat (asm/fcntl.h)
	.set O_RDONLY, 0

	#flags for lseek (stdio.h)
	.set SEEK_SET, 0
	.set SEEK_CUR, 1
	.set SEEK_END, 2

	#protections for mmap (asm/mman.h)
	.set PROT_NONE, 0
	.set PROT_READ, 1
	.set PROT_WRITE, 2
	.set PROT_EXEC, 4
	#flags for mmap (asm/mman.h)
	.set MAP_SHARED, 1
	.set MAP_PRIVATE, 2
	.set MAP_ANONYMOUS, 0x20

	#syscall numbers (asm/unistd.h)
	.set N_exit, 1
	.set N_read, 3
	.set N_write, 4
	.set N_open, 5
	.set N_close, 6
	.set N_lseek, 19
	.set N_mmap, 90
	.set N_munmap, 91
	.set N_mprotect, 125


# Variables
	.section .sdata, "aw", @progbits

	.macro sd_label name
	.set \name, . - .sdata
	.endm

	.macro sd_int name, value
	sd_label \name
	.int \value
	.endm

	sd_int fd, 0
	sd_int len, 0
	sd_int image, 0
	sd_int ds_base, 0
	sd_int rs_base, 0

	sd_label sc_tbl
	.int bye, sc0, sc0, sc0 
	.int sc0, sc0, sc0, sc0
	.int sc0, sc0, sc0, sc0
	.int sc0, sc0, sc0, sc0

	.int emit, key, term_color, sc0 
	.int sc0, sc0, sc0, sc0
	.int sc0, sc0, sc0, sc0
	.int sc0, sc0, sc0, sc0

	.int sc0, sc0, sc0, sc0 
	.int sc0, sc0, sc0, sc0
	.int sc0, sc0, sc0, sc0
	.int sc0, sc0, sc0, sc0

	.int sc0, sc0, sc0, sc0 
	.int sc0, sc0, sc0, sc0
	.int sc0, sc0, sc0, sc0
	.int sc0, sc0, sc0, sc0


	.text
	.balign 4
# Error handling

usage:
	li r3,254
	b exit

open_error:
close_error:
lseek_error:
read_error:
mmap_error:
munmap_error:
mprotect_error:
	li r3,255
exit:
	li r0,N_exit
	sc
0:	b 0b


# Syscall

syscall:
	rlwinm T,T,2,24,29
	add r3,r13,T
	lwz r3,sc_tbl(r3)
	lwzu T,-4(DS)
	mtlr r3
	blr

bye:
	stwu T,-4(r1)
	bl unload_image
	bl cleanup_sp
	bl cleanup_rp
	lwz r3,0(r1)
	b exit

emit:
	li r3,stdout
	addi r4,DS,3
	li r5,1
	li r0,N_write
	sc
	lwzu T,-4(DS)
	b next

term_color:
	li r3,stdout
	mr r4,DS
	lwz r5,0(r4)
	addi r5,r5,0x30
	lis r6,0x1b5b
	ori r6,r6,0x3300
	rlwimi r5,r6,0,0,23
	stw r5,0(r4)
	lis r5,0x6d00
	stw r5,4(r4)
	li r5,5
	li r0,N_write
	sc
	lwzu T,-4(DS)
	b next
	

key:
	li r3,stdin
	addi r4,DS,7
	li r5,1
	li r0,N_read
	sc
	stw T,0(DS)
	lwzu T,4(DS)
sc0:
	b next

# Initialization

open_image:
	li r4,O_RDONLY
	li r0,N_open
	sc
	bso open_error
	stw r3,fd(r13)
	blr

close_image:
	lwz r3,fd(r13)
	li r0,N_close
	sc
	bso close_error
	blr

check_image_format:
	stwu r3,-4(r1)
	li r4,0
	li r5,SEEK_END
	li r0,N_lseek
	sc
	bso lseek_error
	stw r3,len(r13)
	lwz r3,0(r1)
	addi r1,r1,4
	li r4,0
	li r5,SEEK_SET
	li r0,N_lseek
	sc
	bso lseek_error
	blr

#in: r4 = len, r5 = protections
#out: r3 = address
map_anonymous:
	li r3,0
	addi r4,r4,4095
	clrrwi r4,r4,12
	li r6,MAP_SHARED|MAP_ANONYMOUS
	li r7,0 #fd, unused for MAP_ANONYMOUS
	li r8,0 #offset, unused for MAP_ANONYMOUS
	li r0,N_mmap
	sc
	bso mmap_error
	blr

#in: none
#out: r3 = address
alloc_image_mem:
	lwz r4,len(r13)
	li r5,PROT_READ|PROT_WRITE
	b map_anonymous

#in: r3 = address to read at
read_image:
	mr r4,r3
	lwz r3,fd(r13)
	lwz r5,len(r13)
	li r0,N_read
	sc
	bso read_error
	lwz r4,len(r13)
	cmpw r3,r4
	bne read_error
	blr

load_image:
	lwz r3,0(r1)
	cmpwi r3,2
	bne usage
	lwz r3,8(r1)
	addi r1,r1,16  #drop cmdline args
	prolog
	bl open_image
	bl check_image_format
	bl alloc_image_mem
	stw r3,image(r13)
	mr BP,r3
	bl read_image
	bl close_image
	epilog

unload_image:
	lwz r3,image(r13)
	lwz r4,len(r13)
	li r0,N_munmap
	sc
	bso munmap_error
	blr

deny_page:
	li r4,4096
	li r5,PROT_NONE
	li r0,N_mprotect
	sc
	bso mprotect_error
	blr

#in: r3 = length (must be multiple of page size [4096])
#out: r3 = address
alloc_guarded:
	prolog
	stw r3,4(r1)
	#add guard pages
	addi r4,r3,2*4096
	li r5,PROT_READ|PROT_WRITE
	bl map_anonymous
	stw r3,8(r1)
	bl deny_page
	#compute and save start of usable memory
	lwz r3,8(r1)
	addi r3,r3,4096
	stw r3,12(r1)
	#compute start of high guard page
	lwz r6,4(r1)
	add r3,r3,r6
	bl deny_page
	lwz r3,12(r1)
	epilog

init_sp:
	prolog
	li r3,4096
	bl alloc_guarded
	stw r3,ds_base(r13)
	mr DS,r3
	epilog

init_rp:
	prolog
	li r3,8192
	bl alloc_guarded
	stw r3,rs_base(r13)
	mr RS,r3
	epilog

cleanup_sp:
	lwz r3,ds_base(r13)
	li r4,4096
	li r0,N_munmap
	sc
	bso munmap_error
	blr

cleanup_rp:
	lwz r3,rs_base(r13)
	li r4,4096
	li r0,N_munmap
	sc
	bso munmap_error
	blr


	.global _start  # just to shut up the stupid linker.
_start:
	lwi r13,.sdata
	bl load_image
	li T,0
	mr IP,BP
	lwz IL,0(IP)
	addi IP,IP,4
	lwi IT,itbl
	bl init_sp
	bl init_rp
	li FS,0
	li A,0
	#drop through to interpreter step
next:
	rlwimi IT,IL,4,22,27  #IT[IL&0x3f]
	srwi IL,IL,6  #shift latch down by 6 bits
	mtlr IT
	blr

# Helper Routines

push_nCA:
	adde FS,FS,FS
	xori FS,FS,1
	b next

# Instructions that don't fit in the table

call:
	sub IP,IP,BP
	stw IP,0(RS)
	addi RS,RS,4
	add IP,IL,BP
	lwz IL,0(IP)
	addi IP,IP,4
	b next

rot:
	lwz r0,-8(DS)
	lwz r3,-4(DS)
	stw T,-4(DS)
	stw r3,-8(DS)
	mr T,r0
	b next

lit:
	stw T,0(DS)
	addi DS,DS,4
	lwz T,0(IP)
	addi IP,IP,4
	b next

branch:
	lwz IP,0(IP)
	add IP,IP,BP
	lwz IL,0(IP)
	addi IP,IP,4
	b next

slash_mod: #Traditional order ( a b -- rem quot )
	lwz r0,-4(DS)
	divw r3,r0,T
	mullw r4,r3,T
	subf r4,r4,r0
	mr T,r3
	stw r4,-4(DS)
	b next

	#Word name order ( a b -- quot rem )
	#lwz r0,-4(DS)
	#divw r3,r0,T
	#mullw T,r3,T
	#subf T,T,r0
	#stw r3,-4(DS)
	#b next


# **********************************************************************
# Instruction Table
# **********************************************************************

	.section .text.itbl, "ax", @progbits

	# align to 1KB boundary
	# 64 primitives (4 words each).
	.balign 1024
itbl:

#op 0 latch
latch:
	lwz IL,0(IP)
	addi IP,IP,4
	b next
	nop

#op  1 dup
dup:
	stw T,0(DS)
	addi DS,DS,4
	b next
	nop

#op  2 call
call_:
	b call
	nop
	nop
	nop

#op  3 lit
lit_:
	b lit
	nop
	nop
	nop

#op  4 drop
drop:
	lwzu T,-4(DS)
	b next
	nop
	nop

#op  5 swap
swap:
	lwz r0,-4(DS)
	stw T,-4(DS)
	mr T,r0
	b next

#op  6 over
over:
	stw T,0(DS)
	lwz T,-4(DS)
	addi DS,DS,4
	b next

#op  7 nip
nip:
	subi DS,DS,4
	b next
	nop
	nop

#op  8 rot
rot_:
	b rot
	nop
	nop
	nop

#op  9 >r
to_r_:
	stw T,0(RS)
	addi RS,RS,4
	lwzu T,-4(DS)
	b next

#op 10 >>r
dup_to_r:
	stw T,0(RS)
	addi RS,RS,4
	b next
	nop

#op 11 r@
r_fetch:
	stw T,0(DS)
	addi DS,DS,4
	lwz T,-4(RS)
	b next

#op 12 r>
r_from:
	stw T,0(DS)
	addi DS,DS,4
	lwzu T,-4(RS)
	b next

#op 13 rdrop
rdrop:
	subi RS,RS,4
	b next
	nop
	nop

#op 14 ;
return:
	lwzu IP,-4(RS)
	add IP,IP,BP
	b latch
	nop

#op 15 b
branch_:
	b branch
	nop
	nop
	nop

#op 16 ?b
qbranch:
	rotrwi. FS,FS,1
	blt branch
	addi IP,IP,4
	b next

#op 17 0b
zbranch:
	rotrwi. FS,FS,1
	bge branch
	addi IP,IP,4
	b next

#op 18 ?;
qreturn:
	rotrwi. FS,FS,1
	blt return
	b next
	nop

#op 19 0;
zreturn:
	rotrwi. FS,FS,1
	bge return
	b next
	nop

#op 20 t;
treturn:
	andi. r0,FS,1
	bne return
	rotrwi FS,FS,1
	b next

#op 21 f;
freturn:
	andi. r0,FS,1
	beq return
	rotrwi FS,FS,1
	b next

#op 22 ?
question:
	addic r0,T,-1
push_CA:
	adde FS,FS,FS
	b next
	nop

#op 23 0=
zequals:
	not r0,T
	addic r0,r0,1
	lwzu T,-4(DS)
	b push_CA

#op 24 =
equals:
	lwzu r0,-4(DS)
	subf T,T,r0
	b zequals
	nop

#op 25 <
less:
	lwz r0,-4(DS)
	subfc r0,T,r0
	lwzu T,-8(DS)
	b push_nCA

#op 26 &
andfl:
	clrlwi r0,FS,31
	rotrwi FS,FS,1
	and FS,FS,r0
	b next

#op 27 |
orfl:
	clrlwi r0,FS,31
	rotrwi FS,FS,1
	or FS,FS,r0
	b next

#op 28 ^
xorfl:
	clrlwi r0,FS,31
	rotrwi FS,FS,1
	xor FS,FS,r0
	b next

#op 29 ~
notfl:
	xori FS,FS,1
	b next
	nop
	nop

#op 30 and
and:
	lwzu r0,-4(DS)
	and T,T,r0
	b next
	nop

#op 31 or
or:
	lwzu r0,-4(DS)
	or T,T,r0
	b next
	nop

#op 32 xor
xor:
	lwz r0,-4(DS)
	xor T,T,r0
	b next
	nop

#op 33 not
not:
	not T,T
	b next
	nop
	nop

#op 34 >>
rshift:
	lwzu r0,-4(DS)
	srw T,r0,T
	b next
	nop

#op 35 s>>
srshift:
	lwzu r0,-4(DS)
	sraw T,r0,T
	b next
	nop

#op 36 <<
lshift:
	lwzu r0,-4(DS)
	slw T,r0,T
	b next
	nop

#op 37 <<>
lrot:
	lwzu r0,-4(DS)
	rotlw T,r0,T
	b next
	nop

#op 38 +
plus:
	lwzu r0,-4(DS)
	add T,T,r0
	b next
	nop

#op 39 -
minus:
	lwzu r0,-4(DS)
	subf T,T,r0
	b next
	nop

#op 40 *
star:
	lwzu r0,-4(DS)
	mullw T,T,r0
	b next
	nop

#op 41 /
slash:
	lwzu r0,-4(DS)
	divw T,r0,T
	b next
	nop

#op 42 /mod
slash_mod_:
	b slash_mod
	nop
	nop
	nop

#op 43 1+
oneplus:
	addi T,T,1
	b next
	nop
	nop

#op 44 1-
oneminus:
	subi T,T,1
	b next
	nop
	nop

#op 45 4+
fourplus:
	addi T,T,4
	b next
	nop
	nop

#op 46 4-
fourminus:
	subi T,T,4
	b next
	nop
	nop

#op 47 4*
fourstar:
	slwi T,T,2
	b next
	nop
	nop

#op 48 8+
eightplus:
	addi T,T,8
	b next
	nop
	nop

#op 49 >a
to_a:
	add A,T,BP
	lwzu T,-4(DS)
	b next
	nop

#op 50 a
a:
	stw T,0(DS)
	addi DS,DS,4
	sub T,A,BP
	b next

#op 51 @a
fetch_a:
	stw T,0(DS)
	addi DS,DS,4
	lwz T,0(A)
	b next

#op 52 !a
store_a:
	stw T,0(A)
	lwzu T,-4(DS)
	b next
	nop

#op 53 +@
plus_fetch:
	stw T,0(DS)
	addi DS,DS,4
	lwzu T,4(A)
	b next

#op 54 b+@
b_plus_fetch:
	stw T,0(DS)
	addi DS,DS,4
	lbzu T,1(A)
	b next

#op 55 +!
plus_store:
	stwu T,4(A)
	lwzu T,-4(DS)
	b next
	nop

#op 56 b+!
b_plus_store:
	stbu T,1(A)
	lwzu T,-4(DS)
	b next
	nop

#op 57 @
fetch:
	lwzx T,T,BP
	b next
	nop
	nop

#op 58 !
store:
	lwz r0,-4(DS)
	stwx r0,T,BP
	lwzu T,-8(DS)
	b next

#op 59 h@
h_fetch:
	lhzx T,T,BP
	b next
	nop
	nop

#op 60 h!
h_store:
	lwz r0,-4(DS)
	sthx r0,T,BP
	lwzu T,-8(DS)
	b next

#op 61 b@
b_fetch:
	lbzx T,T,BP
	b next
	nop
	nop

#op 62 b!
b_store:
	lwz r0,-4(DS)
	stbx r0,T,BP
	lwzu T,-8(DS)
	b next

#op 63 syscall
syscall_:
	b syscall
