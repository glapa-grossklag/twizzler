CONFIG_DEBUG=n
CONFIG_WERROR=y
CONFIG_OPTIMIZE=g

CONFIG_ARCH=x86_64
CONFIG_MACHINE=pc

CONFIG_UBSAN=n

CONFIG_INSTRUMENT=n

# set this to your toolchain path
TOOLCHAIN_PATH=/home/dbittman/code/twizzler-kernel/.toolchain
QEMU_FLAGS+="-enable-kvm"
