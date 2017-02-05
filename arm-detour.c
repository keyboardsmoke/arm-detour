#include <sys/mman.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>

#include "arm-detour.h"

struct BRANCH_INST
{
	unsigned signed_immed_24 : 24;
	unsigned L : 1;
	unsigned S101 : 3;
	unsigned Cond : 4;
};

static_assert(sizeof(BRANCH_INST) == 4, "Invalid ARM branch size.");

int set_page_protections_for_address(void *addr, size_t page_size, int prot)
{
	void *page_start = (void *)((uintptr_t) addr & -page_size);
	
	return mprotect(page_start, page_size, prot);
}

void create_unconditional_branch(void *src, void *dest, BRANCH_INST *out)
{
	out->L = 0; // Don't store return address
	out->Cond = 0b1110; // unconditional branch instruction
	out->S101 = 0b101; // static
	
	uintptr_t pc = ((uintptr_t) src + 8);
	uintptr_t offset = ((uintptr_t)dest - pc);
	uintptr_t final = offset / 4;
	out->signed_immed_24 = final;
}

void *detour(void *src, void *dest)
{
	long page_size = sysconf(_SC_PAGESIZE);
	
	if (set_page_protections_for_address(src, page_size, PROT_READ | PROT_WRITE | PROT_EXEC) == -1) {
		printf("Failed to mprotect src... (%d, 0x%x)\n", errno, errno);
		return 0;
	}
	
	BRANCH_INST *trampoline = (BRANCH_INST *) malloc(page_size);
	if (!trampoline) {
		printf("Failed to map trampoline...\n");
		return 0;
	}
	
	if (set_page_protections_for_address(trampoline, page_size, PROT_READ | PROT_WRITE | PROT_EXEC) == -1) {
		printf("Failed to mprotect alloc... (%d, 0x%x)\n", errno, errno);
		return 0;
	}
	
	// Copy the old instruction to the trampoline
	memcpy(&trampoline[0], src, sizeof(BRANCH_INST));
	
	// Create an unconditional jmp to the hook dest
	BRANCH_INST ForceJumpInstruction;
	create_unconditional_branch(src, dest, &ForceJumpInstruction);
	
	// Add an unconditional jump from our trampoline to the second instruction in src
	BRANCH_INST trampolineFinal;
	create_unconditional_branch(&trampoline[1], (void *)((uintptr_t)src + 4), &trampolineFinal);
	memcpy(&trampoline[1], &trampolineFinal, sizeof(BRANCH_INST));
	
	// Overwrite the first instruction LAST so we aren't hitting execution in the trampoline before it's full
	// We also use an integer operation here because it should be atomic on aligned accesses
	*(int *) src = *(int *) &ForceJumpInstruction;
	
	return (void *) trampoline;
}