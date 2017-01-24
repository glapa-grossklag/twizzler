#include <interrupt.h>
#include <thread.h>
#include <syscall.h>

long syscall_null(void)
{
	printk("-- called null syscall\n");
	return 0;
}

long (*syscall_table[NUM_SYSCALLS])() = {
	[0] = syscall_null,
	[1] = syscall_null,
	[2] = syscall_null,
	[3] = syscall_null,
	[4] = syscall_null,
	[5] = syscall_null,
	[6] = syscall_null,
	[7] = syscall_null,
};

