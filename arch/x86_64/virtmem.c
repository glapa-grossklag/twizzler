#include <memory.h>
#include <processor.h>

#define PML4_IDX(v) (((v) >> 39) & 0x1FF)
#define PDPT_IDX(v) (((v) >> 30) & 0x1FF)
#define PD_IDX(v)   (((v) >> 21) & 0x1FF)
#define PT_IDX(v)   (((v) >> 12) & 0x1FF)
#define PAGE_PRESENT (1ull << 0)
#define PAGE_LARGE   (1ull << 7)

#define RECUR_ATTR_MASK (VM_MAP_EXEC | VM_MAP_USER | VM_MAP_WRITE | VM_MAP_ACCESSED | VM_MAP_DIRTY)

static inline void test_and_allocate(uintptr_t *loc, uint64_t attr)
{
	if(!*loc) {
		*loc = (uintptr_t)mm_physical_alloc(0x1000, PM_TYPE_DRAM, true) | (attr & RECUR_ATTR_MASK) | PAGE_PRESENT;
	}
}

#define GET_VIRT_TABLE(x) ((uintptr_t *)(((x) & VM_PHYS_MASK) + PHYSICAL_MAP_START))


bool arch_vm_map(struct vm_context *ctx, uintptr_t virt, uintptr_t phys, int level, uint64_t flags)
{
	/* translate flags for NX bit (toggle) */
	flags ^= VM_MAP_EXEC;

	int pml4_idx = PML4_IDX(virt);
	int pdpt_idx = PDPT_IDX(virt);
	int pd_idx   = PD_IDX(virt);
	int pt_idx   = PT_IDX(virt);

	if(ctx == NULL) {
		ctx = current_thread->ctx;
	}

	uintptr_t *pml4 = GET_VIRT_TABLE(ctx->arch.pml4_phys);
	test_and_allocate(&pml4[pml4_idx], flags);
	
	uintptr_t *pdpt = GET_VIRT_TABLE(pml4[pml4_idx]);
	if(level == 2) {
		if(pdpt[pdpt_idx]) {
			return false;
		}
		pdpt[pdpt_idx] = phys | flags | PAGE_PRESENT | PAGE_LARGE;
	} else {
		test_and_allocate(&pdpt[pdpt_idx], flags);
		uintptr_t *pd = GET_VIRT_TABLE(pdpt[pdpt_idx]);

		if(level == 1) {
			if(pd[pd_idx]) {
				return false;
			}
			pd[pd_idx] = phys | flags | PAGE_PRESENT | PAGE_LARGE;
		} else {
			test_and_allocate(&pd[pd_idx], flags);
			uintptr_t *pt = GET_VIRT_TABLE(pd[pd_idx]);
			if(pt[pt_idx]) {
				return false;
			}
			pt[pt_idx] = phys | flags | PAGE_PRESENT;
		}
	}
	return true;
}

#include <object.h>
#define MB (1024ul * 1024ul)
void arch_vm_map_object(struct vm_context *ctx, struct vmap *map, struct object *obj)
{
	if(obj->slot == -1) {
		panic("tried to map an unslotted object");
	}
	uintptr_t vaddr = map->slot * mm_page_size(MAX_PGLEVEL);
	uintptr_t oaddr = obj->slot * mm_page_size(obj->pglevel);
	for(int i=0;i<512;i++) {
		if(arch_vm_map(ctx, vaddr + i * (2*MB), oaddr + i * (2*MB), 1, VM_MAP_USER | VM_MAP_EXEC | VM_MAP_WRITE) == false) panic("remap failed"); //TODO: fix flags
	}
}

uint64_t *kernel_pml4;
extern uint64_t boot_pml4;
#define PHYS_LOAD_ADDRESS (KERNEL_PHYSICAL_BASE + KERNEL_LOAD_OFFSET)
#define PHYS_ADDR_DELTA (KERNEL_VIRTUAL_BASE + KERNEL_LOAD_OFFSET - PHYS_LOAD_ADDRESS)
#define PHYS(x) ((x) - PHYS_ADDR_DELTA)
void arch_mm_switch_context(struct vm_context *ctx)
{
	if(ctx == NULL) {
		asm volatile("mov %0, %%cr3" :: "r"(PHYS((uintptr_t)&boot_pml4)) : "memory");
	} else {
		asm volatile("mov %0, %%cr3" :: "r"(ctx->arch.pml4_phys) : "memory");
	}
}

__initializer
static void _x86_64_init_vm(void)
{
	kernel_pml4 = &boot_pml4;
	/* TODO: we should re-map with better flags and stuff */
}

void arch_mm_context_init(struct vm_context *ctx)
{
	ctx->arch.pml4_phys = mm_physical_alloc(0x1000, PM_TYPE_DRAM, true);
	uint64_t *pml4 = (uint64_t *)(ctx->arch.pml4_phys + PHYSICAL_MAP_START);
	for(int i=0;i<256;i++) {
		pml4[i] = 0;
	}
	for(int i=256;i<512;i++) {
		pml4[i] = kernel_pml4[i];
	}
}

