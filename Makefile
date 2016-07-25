
CONFIGFILE=config.mk
BUILDDIR=build

include $(CONFIGFILE)
DEFINES=$(addprefix -D,$(shell cat $(CONFIGFILE) | sed -e 's/=y/=1/g' -e 's/=n/=0/g' -e 's/\#.*$$//' -e '/^$$/d'))

ARCH=$(CONFIG_ARCH)
MACHINE=$(CONFIG_MACHINE)

INCLUDES=-Iinclude -Imachine/$(MACHINE)/include -Iarch/$(ARCH)/include
CFLAGS=-ffreestanding -Wall -Wextra -std=gnu11 -include stdbool.h -include stddef.h -include stdint.h $(INCLUDES) -include printk.h $(DEFINES) -include system.h -fno-omit-frame-pointer -g
ASFLAGS=$(INCLUDES) $(DEFINES)

ifeq ($(CONFIG_WERROR),y)
CFLAGS+=-Werror
endif

ifeq ($(CONFIG_UBSAN),y)
CFLAGS+=-fsanitize=undefined -fstack-check -fstack-protector-all
endif

CFLAGS+=-O$(CONFIG_OPTIMIZE)

C_SOURCES=
ASM_SOURCES=

OBJECTS=$(addprefix $(BUILDDIR)/,$(ASM_SOURCES:.S=.o) $(C_SOURCES:.c=.o))

CRTBEGIN=$(shell $(TOOLCHAIN_PREFIX)gcc -print-file-name=crtbegin.o)
CRTEND=$(shell $(TOOLCHAIN_PREFIX)gcc -print-file-name=crtend.o)
CRTI=$(shell $(TOOLCHAIN_PREFIX)gcc -print-file-name=crti.o)
CRTN=$(shell $(TOOLCHAIN_PREFIX)gcc -print-file-name=crtn.o)

all: $(BUILDDIR)/kernel

include arch/$(ARCH)/include.mk
include machine/$(MACHINE)/include.mk
include lib/include.mk
include core/include.mk
-include $(OBJECTS:.o=.d)

test: $(BUILDDIR)/kernel
	$(QEMU) $(QEMU_FLAGS) -serial stdio

$(BUILDDIR)/kernel: $(BUILDDIR)/link.ld $(OBJECTS) $(BUILDDIR)/symbols.o
	@mkdir -p $(BUILDDIR)
	@echo "[LD]  $@"
	@$(TOOLCHAIN_PREFIX)gcc -ffreestanding -nostdlib $(CRTBEGIN) $(CRTI) $(OBJECTS) $(BUILDDIR)/symbols.o $(CRTN) $(CRTEND) -o $(BUILDDIR)/kernel -T $(BUILDDIR)/link.ld -lgcc -Wl,--export-dynamic

$(BUILDDIR)/symbols.o: $(BUILDDIR)/symbols.c
	@echo "[CC]  $@"
	@$(TOOLCHAIN_PREFIX)gcc $(CFLAGS) -c -o $@ $<

$(BUILDDIR)/symbols.c: $(BUILDDIR)/kernel.sym.c
	@echo "[GEN] $@"
	@echo '#include <ksymbol.h>' > $(BUILDDIR)/symbols.c
	@echo '__attribute__((section(".ksyms"))) const struct ksymbol kernel_symbol_table[] = {' >> $(BUILDDIR)/symbols.c
	@cat $(BUILDDIR)/kernel.sym.c >> $(BUILDDIR)/symbols.c
	@echo "};" >> $(BUILDDIR)/symbols.c
	@echo >> $(BUILDDIR)/symbols.c
	@echo '__attribute__((section(".ksyms"))) const size_t kernel_symbol_table_length = ' $$(wc -l < $(BUILDDIR)/kernel.sym.c) ';' >> $(BUILDDIR)/symbols.c

$(BUILDDIR)/kernel.sym.c: $(BUILDDIR)/kernel.stage1
	@echo "[GEN] $@"
	@$(TOOLCHAIN_PREFIX)objdump -t $(BUILDDIR)/kernel.stage1 | grep '^.* [lg]' | awk '{print $$1 " " $$(NF-1) " " $$NF}' | grep -v '.hidden' | sed -rn 's|([0-9a-f]+) ([0-9a-f]+) ([a-zA-Z0-9_/\.]+)|{.value=0x\1, .size=0x\2, .name="\3"},|p' > $(BUILDDIR)/kernel.sym.c

$(BUILDDIR)/kernel.stage1: $(BUILDDIR)/link.ld $(OBJECTS)
	@echo "[LD]  $@"
	@mkdir -p $(BUILDDIR)
	@$(TOOLCHAIN_PREFIX)gcc -ffreestanding -nostdlib $(CRTBEGIN) $(CRTI) $(OBJECTS) $(CRTN) $(CRTEND) -o $(BUILDDIR)/kernel.stage1 -T $(BUILDDIR)/link.ld -lgcc -Wl,--export-dynamic

$(BUILDDIR)/%.o : %.S $(CONFIGFILE)
	@echo "[AS]  $@"
	@mkdir -p $(@D)
	@$(TOOLCHAIN_PREFIX)gcc $(ASFLAGS) -c $< -o $@ -MD -MF $(BUILDDIR)/$*.d

$(BUILDDIR)/%.o : %.c $(CONFIGFILE)
	@echo "[CC]  $@"
	@mkdir -p $(@D)
	@$(TOOLCHAIN_PREFIX)gcc $(CFLAGS) -c $< -o $@ -MD -MF $(BUILDDIR)/$*.d

clean:
	-rm -rf $(BUILDDIR)

od: $(BUILDDIR)/kernel
	$(TOOLCHAIN_PREFIX)objdump -dS $(BUILDDIR)/kernel

re: $(BUILDDIR)/kernel
	$(TOOLCHAIN_PREFIX)readelf -a $(BUILDDIR)/kernel

.PHONY: od re clean all test

