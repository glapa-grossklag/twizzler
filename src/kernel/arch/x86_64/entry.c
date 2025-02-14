/*
 * SPDX-FileCopyrightText: 2021 Daniel Bittman <danielbittman1@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch/x86_64-msr.h>
#include <arch/x86_64-vmx.h>
#include <clksrc.h>
#include <kalloc.h>
#include <kheap.h>
#include <processor.h>
#include <secctx.h>
#include <syscall.h>
#include <thread.h>
#include <vmm.h>

void x86_64_signal_eoi(void);

/* NOTE: Twizzler's TLB shootdown method leaves a lot to be desired. However, remember that our goal
 * is _not_ to research good TLB shootdown algs (at least, not yet ;) ). */
void x86_64_ipi_tlb_shootdown(void)
{
	asm volatile("mov %%cr3, %%rax; mov %%rax, %%cr3; mfence;" ::: "memory", "rax");
	int x;
	/* TODO: better invalidation scheme */
	/* TODO: separate IPI for this? */
	asm volatile("invept %0, %%rax" ::"m"(x), "r"(0));
	if(current_processor)
		current_processor->stats.shootdowns++;
	processor_ipi_finish();
}

void x86_64_ipi_resume(void)
{
	/* don't need to do anything, just handle the interrupt */
}

void x86_64_ipi_halt(void)
{
	processor_shutdown();
	asm volatile("cli; jmp .;" ::: "memory");
}

/*
static void x86_64_change_fpusse_allow(bool enable)
{
    register uint64_t tmp;
    asm volatile("mov %%cr0, %0" : "=r"(tmp));
    tmp = enable ? (tmp & ~(1 << 2)) : (tmp | (1 << 2));
    asm volatile("mov %0, %%cr0" ::"r"(tmp));
    asm volatile("mov %%cr4, %0" : "=r"(tmp));
    tmp = enable ? (tmp | (1 << 9)) : (tmp & ~(1 << 9));
    asm volatile("mov %0, %%cr4" ::"r"(tmp));
}
*/

__noinstrument void x86_64_exception_entry(struct x86_64_exception_frame *frame,
  bool was_userspace,
  bool ignored)
{
	// if(frame->int_no != 32)
	//	printk(":: e: %ld\n", frame->int_no);
	if(!ignored) {
		if(was_userspace) {
			current_thread->arch.was_syscall = false;
		}

		if((frame->int_no == 6 || frame->int_no == 7)) {
			if(!was_userspace) {
				panic("floating-point operations used in kernel-space");
			}
			panic("NI - FP exception");
		} else if(frame->int_no == 14) {
			/* page fault */
			uint64_t cr2;
			asm volatile("mov %%cr2, %0" : "=r"(cr2)::"memory");
			int flags = 0;
			if(frame->err_code & (1 << 3)) {
				goto pf_bad;
			}
			if(frame->err_code & 1) {
				flags |= FAULT_ERROR_PERM;
			} else {
				flags |= FAULT_ERROR_PRES;
			}
			if(frame->err_code & (1 << 1)) {
				flags |= FAULT_WRITE;
			}
			if(frame->err_code & (1 << 2)) {
				flags |= FAULT_USER;
			}
			if(frame->err_code & (1 << 4)) {
				flags |= FAULT_EXEC;
			}
			if(!was_userspace && !VADDR_IS_USER(cr2)) {
			pf_bad:
				panic("kernel-mode page fault to address %lx\n"
				      "    from %lx: %s while in %s-mode: %s %s\n",
				  cr2,
				  frame->rip,
				  flags & FAULT_EXEC ? "ifetch" : (flags & FAULT_WRITE ? "write" : "read"),
				  flags & FAULT_USER ? "user" : "kernel",
				  flags & FAULT_ERROR_PERM ? "present" : "non-present",
				  frame->err_code & (1 << 3) ? "(RESERVED)" : "");
			}
			kernel_fault_entry(frame->rip, cr2, flags);
		} else if(frame->int_no == 20) {
			/* #VE */
			x86_64_virtualization_fault(current_processor);
		} else if(frame->int_no == 18) {
			panic("!! MCE: %lx\n", frame->rip);
		} else if(frame->int_no < 32) {
			if(was_userspace) {
				struct fault_exception_info info = twz_fault_build_exception_info(
				  (void *)frame->rip, frame->int_no, frame->err_code, 0);
				if(frame->int_no == 19) {
					/* SIMD exception; get info from MXCSR */
					asm volatile("stmxcsr %0" : "=m"(info.arg0));
				}
				thread_raise_fault(current_thread, FAULT_EXCEPTION, &info, sizeof(info));
			} else {
				if(frame->int_no == 3) {
					printk("[debug]: recv debug interrupt\n");
					kernel_debug_entry();
				}
				panic("kernel exception: %ld, from %lx\n", frame->int_no, frame->rip);
			}
		} else if(frame->int_no == PROCESSOR_IPI_HALT) {
			x86_64_ipi_halt();
		} else if(frame->int_no == PROCESSOR_IPI_SHOOTDOWN) {
			x86_64_ipi_tlb_shootdown();
		} else if(frame->int_no == PROCESSOR_IPI_RESUME) {
			x86_64_ipi_resume();
		} else {
#if 0
			printk("INTERRUPT : %ld (%d ; %d)\n",
			  frame->int_no,
			  was_userspace,
			  current_thread->arch.was_syscall);
#endif
			kernel_interrupt_entry(frame->int_no);
		}
	}

	if(frame->int_no >= 32) {
		if(current_thread && current_thread->processor)
			current_thread->processor->stats.ext_intr++;
		x86_64_signal_eoi();
	} else if(current_thread && current_thread->processor) {
		current_thread->processor->stats.int_intr++;
	}
	// printk(":: %ld %d %d :: %lx\n", frame->int_no, was_userspace, ignored, frame->rip);
	if(was_userspace) {
		//	printk("returning to userspace from interrupt to %lx\n", frame->rip);
		thread_schedule_resume();
	}
	/* if we aren't in userspace, we just return and the kernel_exception handler will
	 * properly restore the frame and continue */
}

extern long (*syscall_table[])();
__noinstrument void x86_64_syscall_entry(struct x86_64_syscall_frame *frame)
{
	long num = frame->rax;
	// long xx = krdtsc();
#if CONFIG_PRINT_SYSCALLS || 1
	// long long a = krdtsc();
#endif
	current_thread->arch.was_syscall = true;
	arch_interrupt_set(true);
	current_thread->processor->stats.syscalls++;
	if(frame->rax < NUM_SYSCALLS) {
		if(syscall_table[frame->rax]) {
			long r = syscall_prelude(frame->rax);
			if(!r) {
				frame->rax = syscall_table[frame->rax](
				  frame->rdi, frame->rsi, frame->rdx, frame->r8, frame->r9, frame->r10);
			} else {
				frame->rax = r;
			}
			r = syscall_epilogue(frame->rax);
			if(r) {
				panic("NI - non-zero return code from syscall epilogue");
			}
		}
	} else {
		frame->rax = -EINVAL;
	}

#if CONFIG_PRINT_SYSCALLS
	// long long b = krdtsc();
	long long b = 0, a = 0;
	// if(num != SYS_DEBUG_PRINT)
	printk("%ld: SYSCALL %ld (%lx) -> ret %ld took %lld cyc\n",
	  current_thread->id,
	  num,
	  frame->rcx,
	  frame->rax,
	  b - a);
#endif
	// long long xxy = krdtsc();
	// if(num == SYS_BECOME)
	//	printk(":: become sys %ld\n", xxy - xx);
	arch_interrupt_set(false);
	thread_schedule_resume();
}

void secctx_switch(int i)
{
	current_thread->active_sc = current_thread->sctx_entries[i].context;
	if(!current_thread->active_sc) {
		return;
	}
	x86_64_secctx_switch(current_thread->active_sc);
}

extern void x86_64_resume_userspace(void *);
extern void x86_64_resume_userspace_interrupt(void *);
__noinstrument void arch_thread_resume(struct thread *thread, uint64_t timeout)
{
	struct thread *old = current_thread;
	thread->processor->arch.curr = thread;
	thread->processor->arch.tcb =
	  (void *)((uint64_t)&thread->arch.syscall + sizeof(struct x86_64_syscall_frame));
	uint64_t rsp0 = (uint64_t)&thread->arch.exception + sizeof(struct x86_64_exception_frame);
	thread->processor->arch.tss.rsp0 = rsp0;

	// thread->processor->arch.tss.esp0 =
	// ((uint64_t)&thread->arch.exception + sizeof(struct x86_64_exception_frame));

	/* restore segment bases for new thread */
	x86_64_wrmsr(
	  X86_MSR_FS_BASE, thread->arch.fs & 0xFFFFFFFF, (thread->arch.fs >> 32) & 0xFFFFFFFF);
	x86_64_wrmsr(
	  X86_MSR_KERNEL_GS_BASE, thread->arch.gs & 0xFFFFFFFF, (thread->arch.gs >> 32) & 0xFFFFFFFF);

	if(old && old != thread) {
		asm volatile("xsave (%0)" ::"r"(old->arch.xsave_run->start), "a"(7), "d"(0) : "memory");
	}

	if(!thread->arch.fpu_init) {
		thread->arch.fpu_init = true;
		asm volatile("finit;");

		uint16_t fcw;
		asm volatile("fstcw %0" : "=m"(fcw));
		fcw |= 0x33f; // double-prec, mask all
		asm volatile("fldcw %0" : "=m"(fcw));
		uint32_t mxcsr;
		asm volatile("stmxcsr %0; mfence" : "=m"(mxcsr)::"memory");
		mxcsr |= 0x1f80; // mask all
		asm volatile("sfence; ldmxcsr %0" : "=m"(mxcsr)::"memory");
		asm volatile("stmxcsr %0" : "=m"(mxcsr)::"memory");
		/* TODO: fix this: need to properly init the xsave area */
		asm volatile("xsave (%0)" ::"r"(thread->arch.xsave_run->start), "a"(7), "d"(0) : "memory");
	}
	if(old != thread) {
		asm volatile("xrstor (%0)" ::"r"(thread->arch.xsave_run->start), "a"(7), "d"(0) : "memory");
		if((!old || old->ctx != thread->ctx) && thread->ctx) {
			arch_mm_switch_context(thread->ctx);
		}
		if((!old || old->active_sc != thread->active_sc) && thread->active_sc) {
			x86_64_secctx_switch(thread->active_sc);
			thread->processor->stats.sctx_switch++;
		}
	}

	spinlock_acquire_save(&thread->lock);
	if(thread->pending_fault_info) {
		printk("RAISE PENDING\n");
		thread_raise_fault(
		  thread, thread->pending_fault, thread->pending_fault_info, thread->pending_fault_infolen);
		if(thread->pending_fault_info) {
			kfree(thread->pending_fault_info);
			thread->pending_fault_info = NULL;
		}
	}
	uint64_t return_addr =
	  thread->arch.was_syscall ? thread->arch.syscall.rcx : thread->arch.exception.rip;
	if(!VADDR_IS_USER(return_addr)) {
		struct fault_exception_info fei =
		  twz_fault_build_exception_info((void *)return_addr, 14, 1, 0);
		thread_raise_fault(thread, FAULT_EXCEPTION, &fei, sizeof(fei));
	}

	spinlock_release_restore(&thread->lock);

	if(thread->state == THREADSTATE_EXITED) {
		/* we might have exited while we're in this function (e.g. those raise_fault calls!) so
		 * double check this. */
		thread_schedule_resume();
	}

	if(timeout) {
		clksrc_set_interrupt_countdown(timeout, false);
	}
	if(thread->arch.was_syscall) {
		if(thread->state == THREADSTATE_EXITED) {
			/* thread exited! */
		}
		// printk("returning to userspace from syscall to %lx\n", thread->arch.syscall.rcx);
		x86_64_resume_userspace(&thread->arch.syscall);
	} else {
		// printk("returning to us from int to %lx\n", thread->arch.exception.rip);
		x86_64_resume_userspace_interrupt(&thread->arch.exception);
	}
}
