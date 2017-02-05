#include <stdio.h>
#include <stdint.h>

#include "arm-detour.h"

void hook_fn()
{
	printf("Hooked!\n");
}

void normal_fn()
{
	printf("Normal...\n");
}

typedef void (*fn_t)();

int main(int argc, char **argv)
{
	normal_fn();
	
	fn_t orig = (fn_t) detour((void *)normal_fn, (void *)hook_fn);
	if (!orig)
		printf("Failed to hook normal function.\n");
	else {
		printf("Hook success!\n");
		normal_fn();
		printf("Calling the original... (0x%x, 0x%x)\n", *(uintptr_t*) orig, *(uintptr_t *)((uintptr_t) orig + 4));
		orig();
	}
	
	return 0;
}