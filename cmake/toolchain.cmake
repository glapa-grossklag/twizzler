set(REBUILD_TOOLCHAIN ON)

ExternalProject_Add(
	"llvm"
	CMAKE_ARGS
	"CC=clang"
	"CXX=clang++"
	CMAKE_CACHE_ARGS
	"-DCMAKE_BUILD_TYPE:STRING=Release"
	"-DCMAKE_INSTALL_PREFIX:STIRNG=${TOOLCHAIN_DIR}"
	"-DLLVM_ENABLE_PROJECTS:STRING=clang;lld"
	"-DLLVM_TARGETS_TO_BUILD:STRING=X86"
	"-DLLVM_BUILD_TOOLS:BOOL=ON"
	"-DLLVM_BUILD_UTILS:BOOL=OFF"
	"-DLLVM_BUILD_RUNTIME:BOOL=OFF"
	"-DLLVM_INCLUDE_TESTS:BOOL=OFF"
	"-DLLVM_INCLUDE_UTILS:BOOL=OFF"
	"-DLLVM_INCLUDE_RUNTIMES:BOOL=OFF"
	"-DLLVM_INSTALL_TOOLCHAIN_ONLY:BOOL=OFF"
	"-DCLANG_INCLUDE_TESTS:BOOL=OFF"
	"-DLLVM_INSTALL_BINUTILS_SYMLINKS:STRING=ON"
	"-DLLVM_DEFAULT_TARGET_TRIPLE:STRING=${TWIZZLER_TRIPLE}"
	SOURCE_DIR
	"${CMAKE_SOURCE_DIR}/llvm-project/llvm"
	USES_TERMINAL_CONFIGURE ON
	USES_TERMINAL_BUILD ON
	USES_TERMINAL_INSTALL ON
	LOG_INSTALL ON
	BUILD_ALWAYS ${REBUILD_TOOLCHAIN}
	)

ExternalProject_Add_Step(llvm
	post_install
	DEPENDEES install
	USES_TERMINAL ON
	COMMAND
	  "install"
	  "-C"
	  "-D"
	  "-m"
	  "0644"
	  "${CMAKE_SOURCE_DIR}/../src/share/cmake/Modules/Platform/Twizzler.cmake"
	  "${SYSROOT_DIR}/usr/share/cmake/Modules/Platform/Twizzler.cmake"
	COMMAND
	  "install"
	  "-C"
	  "-D"
	  "-m"
	  "0644"
	  "${CMAKE_SOURCE_DIR}/../src/lib/ldscripts/elf_${TWIZZLER_PROCESSOR}_twizzler.x"
	  "${SYSROOT_DIR}/usr/lib/ldscripts/elf_${TWIZZLER_PROCESSOR}_twizzler.x"
	COMMAND
	  "install"
	  "-C"
	  "-D"
	  "-m"
	  "0644"
	  "${CMAKE_SOURCE_DIR}/../src/lib/ldscripts/elf_${TWIZZLER_PROCESSOR}_twizzler.xs"
	  "${SYSROOT_DIR}/usr/lib/ldscripts/elf_${TWIZZLER_PROCESSOR}_twizzler.xs"
	DEPENDS
	  "${CMAKE_SOURCE_DIR}/../src/share/cmake/Modules/Platform/Twizzler.cmake"
	  "${CMAKE_SOURCE_DIR}/../src/lib/ldscripts/elf_${TWIZZLER_PROCESSOR}_twizzler.x"
	  "${CMAKE_SOURCE_DIR}/../src/lib/ldscripts/elf_${TWIZZLER_PROCESSOR}_twizzler.xs"
	  ALWAYS ${REBUILD_TOOLCHAIN}
)

ExternalProject_Add(
	"musl-headers"
	DEPENDS "llvm"
	CONFIGURE_COMMAND "${CMAKE_SOURCE_DIR}/../src/lib/musl/configure"
	"CC=${TOOLCHAIN_DIR}/bin/clang"
	"LD=${TOOLCHAIN_DIR}/bin/clang"
	"ARCH=x86_64"
	"--host=x86_64-pc-twizzler-musl"
	"CFLAGS=-target ${TWIZZLER_TRIPLE} --sysroot ${SYSROOT_DIR} -Wno-ignored-optimization-argument"
	"LDFLAGS=-target ${TWIZZLER_TRIPLE} --sysroot ${SYSROOT_DIR}"
	"--prefix=/usr"
	"--syslibdir=/lib"
	"--enable-debug"
	"--enable-optimize"
	BUILD_COMMAND "make" "-s" "obj/include/bits/alltypes.h" "obj/include/bits/syscall.h"
	INSTALL_COMMAND "make" "-s" "install-headers" "DESTDIR=${SYSROOT_DIR}"
	SOURCE_DIR "${CMAKE_SOURCE_DIR}/../src/lib/musl"
	USES_TERMINAL_CONFIGURE ON
	USES_TERMINAL_BUILD ON
	LOG_CONFIGURE ON
	USES_TERMINAL_INSTALL ON
	BUILD_ALWAYS ${REBUILD_TOOLCHAIN}
)

ExternalProject_Add(
	"compiler-rt"
	DEPENDS "llvm"
	DEPENDS "musl-headers"
	CMAKE_CACHE_ARGS
	"-DCMAKE_BUILD_TYPE:STRING=Release"
	"-DCMAKE_TOOLCHAIN_FILE:STRING=${TOOLCHAIN_FILE}"
	"-DCMAKE_INSTALL_PREFIX:STRING=${TOOLCHAIN_DIR}/lib/clang/12.0.0"
	"-DCMAKE_C_FLAGS:STRING=${CMAKE_C_FLAGS} -nostdlib"
	"-DCMAKE_CXX_FLAGS:STRING=${CMAKE_CXX_FLAGS} -nostdlib"
	"-DCOMPILER_RT_DEFAULT_TARGET_ONLY:BOOL=ON"
	"-DCOMPILER_RT_ENABLE_SHARED:STRING=OFF"
	"-DCOMPILER_RT_USE_LIBCXX:STRING=ON"
	"-DCOMPILER_RT_CRT_USE_EH_FRAME_REGISTRY:STRING=OFF"
	"-DCOMPILER_RT_EXCLUDE_ATOMIC_BUILTIN:STRING=OFF"
	"-DCOMPILER_RT_SANITIZERS_TO_BUILD:STRING="
	"-DCOMPILER_RT_BUILD_SANITIZERS:BOOL=OFF"
	CMAKE_ARGS
	"-DCMAKE_INSTALL_PREFIX:STRING=${TOOLCHAIN_DIR}/lib/clang/12.0.0"
	SOURCE_DIR
	"${CMAKE_SOURCE_DIR}/llvm-project/compiler-rt"
	USES_TERMINAL_CONFIGURE ON
	USES_TERMINAL_BUILD ON
	USES_TERMINAL_INSTALL ON
	LOG_INSTALL ON
	BUILD_ALWAYS ${REBUILD_TOOLCHAIN}
	)

ExternalProject_Add(
	"twz-bootstrap"
	DEPENDS "llvm"
	DEPENDS "musl-headers"
	CMAKE_CACHE_ARGS
	"-DCMAKE_BUILD_TYPE:STRING=Release"
	"-DCMAKE_TOOLCHAIN_FILE:STRING=${TOOLCHAIN_FILE}"
	"-DCMAKE_INSTALL_PREFIX:STRING=${SYSROOT_DIR}/usr"
	"-DCMAKE_C_FLAGS:STRING=${CMAKE_C_FLAGS} -nostdlib"
	"-DCMAKE_CXX_FLAGS:STRING=${CMAKE_CXX_FLAGS} -nostdlib"
	"-DBUILD_SHARED_LIBS:BOOL=OFF"
	SOURCE_DIR
	"${CMAKE_SOURCE_DIR}/../src/lib/twz"
	USES_TERMINAL_CONFIGURE ON
	USES_TERMINAL_BUILD ON
	USES_TERMINAL_INSTALL ON
	LOG_INSTALL ON
	BUILD_ALWAYS ${REBUILD_TOOLCHAIN}
	)

ExternalProject_Add(
	"twix-bootstrap"
	DEPENDS "llvm"
	DEPENDS "musl-headers"
	DEPENDS "twz-bootstrap"
	CMAKE_CACHE_ARGS
	"-DCMAKE_BUILD_TYPE:STRING=Release"
	"-DCMAKE_TOOLCHAIN_FILE:STRING=${TOOLCHAIN_FILE}"
	"-DCMAKE_INSTALL_PREFIX:STRING=${SYSROOT_DIR}/usr"
	"-DCMAKE_C_FLAGS:STRING=${CMAKE_C_FLAGS} -nostdlib"
	"-DCMAKE_CXX_FLAGS:STRING=${CMAKE_CXX_FLAGS} -nostdlib"
	"-DBUILD_SHARED_LIBS:BOOL=OFF"
	SOURCE_DIR
	"${CMAKE_SOURCE_DIR}/../src/lib/twix"
	USES_TERMINAL_CONFIGURE ON
	USES_TERMINAL_BUILD ON
	USES_TERMINAL_INSTALL ON
	LOG_INSTALL ON
	BUILD_ALWAYS ${REBUILD_TOOLCHAIN}
	)

ExternalProject_Add(
	"musl"
	DEPENDS "llvm"
	DEPENDS "twz-bootstrap"
	DEPENDS "twix-bootstrap"
	DEPENDS "compiler-rt"
	CONFIGURE_COMMAND "${CMAKE_SOURCE_DIR}/../src/lib/musl/configure"
	"CC=${TOOLCHAIN_DIR}/bin/clang"
	"LD=${TOOLCHAIN_DIR}/bin/clang"
	"ARCH=x86_64"
	"--host=x86_64-pc-twizzler-musl"
	"CFLAGS=-target ${TWIZZLER_TRIPLE} --sysroot ${SYSROOT_DIR} -Wno-ignored-optimization-argument -Wno-unused-command-line-argument"
	"LDFLAGS=-target ${TWIZZLER_TRIPLE} --sysroot ${SYSROOT_DIR} -Wno-unused-command-line-argument"
	"--prefix=/usr"
	"--syslibdir=/lib"
	"--enable-debug"
	"--enable-optimize"
	BUILD_COMMAND "make" "-s"
		"-j" "${BUILD_CORES}"
	INSTALL_COMMAND "make" "-s" "install" "DESTDIR=${SYSROOT_DIR}"
		"-j" "${BUILD_CORES}"
	SOURCE_DIR "${CMAKE_SOURCE_DIR}/../src/lib/musl"
	USES_TERMINAL_CONFIGURE ON
	USES_TERMINAL_BUILD ON
	USES_TERMINAL_INSTALL ON
	LOG_INSTALL ON
	LOG_CONFIGURE ON
	BUILD_ALWAYS ${REBUILD_TOOLCHAIN}
)

ExternalProject_Add(
	"exttommath"
	DEPENDS "llvm"
	DEPENDS "twz-bootstrap"
	DEPENDS "twix-bootstrap"
	DEPENDS "compiler-rt"
	BUILD_IN_SOURCE ON
	CONFIGURE_COMMAND ""
	BUILD_COMMAND
		"make" "-s"
		"-j" "${BUILD_CORES}"
		"CFLAGS=-target ${TWIZZLER_TRIPLE} --sysroot ${SYSROOT_DIR} -fPIC -O2 -g"
		"CC=${TOOLCHAIN_DIR}/bin/clang"
		"LD=${TOOLCHAIN_DIR}/bin/clang"
		"AR=ar"
		"RANLIB=ranlib"
	INSTALL_COMMAND
		"make" "-s"
		"CFLAGS=-target ${TWIZZLER_TRIPLE} --sysroot ${SYSROOT_DIR} -fPIC -O2 -g"
		"CC=${TOOLCHAIN_DIR}/bin/clang"
		"LD=${TOOLCHAIN_DIR}/bin/clang"
		"AR=ar"
		"RANLIB=ranlib"
		"DESTDIR=${SYSROOT_DIR}"
		"PREFIX=/usr"
		"LIBPATH=/usr/lib"
		"INCPATH=/usr/include"
		"install"
	SOURCE_DIR "${CMAKE_SOURCE_DIR}/libtommath"
	USES_TERMINAL_CONFIGURE ON
	USES_TERMINAL_BUILD ON
	USES_TERMINAL_INSTALL ON
	BUILD_ALWAYS ${REBUILD_TOOLCHAIN}
)

ExternalProject_Add(
	"exttomcrypt"
	DEPENDS "llvm"
	DEPENDS "twz-bootstrap"
	DEPENDS "twix-bootstrap"
	DEPENDS "compiler-rt"
	DEPENDS "exttommath"
	BUILD_IN_SOURCE ON
	CONFIGURE_COMMAND ""
	BUILD_COMMAND
		"make" "-s"
		"-j" "${BUILD_CORES}"
		"CFLAGS=-target ${TWIZZLER_TRIPLE} --sysroot ${SYSROOT_DIR} -fPIC -DUSE_LTM -DLTM_DESC -O2 -g"
		"CC=${TOOLCHAIN_DIR}/bin/clang"
		"LD=${TOOLCHAIN_DIR}/bin/clang"
		"AR=ar"
		"RANLIB=ranlib"
	INSTALL_COMMAND
		"make" "-s"
		"CFLAGS=-target ${TWIZZLER_TRIPLE} --sysroot ${SYSROOT_DIR} -fPIC -DUSE_LTM -DLTM_DESC -O2 -g"
		"CC=${TOOLCHAIN_DIR}/bin/clang"
		"LD=${TOOLCHAIN_DIR}/bin/clang"
		"AR=ar"
		"RANLIB=ranlib"
		"DESTDIR=${SYSROOT_DIR}"
		"LIBPATH=/usr/lib"
		"PREFIX=/usr"
		"INCPATH=/usr/include"
		"install"
	SOURCE_DIR "${CMAKE_SOURCE_DIR}/libtomcrypt"
	USES_TERMINAL_CONFIGURE ON
	USES_TERMINAL_BUILD ON
	USES_TERMINAL_INSTALL ON
	LOG_INSTALL ON
	BUILD_ALWAYS ${REBUILD_TOOLCHAIN}
)

ExternalProject_Add(
    "libunwind"
    DEPENDS "compiler-rt"
    CMAKE_CACHE_ARGS
		"-DCMAKE_BUILD_TYPE:STRING=Debug"
		"-DCMAKE_TOOLCHAIN_FILE:STRING=${TOOLCHAIN_FILE}"
		"-DCMAKE_INSTALL_PREFIX:STRING=${SYSROOT_DIR}/usr"
		"-DCMAKE_C_FLAGS:STRING=${CMAKE_C_FLAGS} -nostdlib -fPIC"
		"-DCMAKE_CXX_FLAGS:STRING=${CMAKE_CXX_FLAGS} -nostdlib -fPIC"
    SOURCE_DIR
        "${CMAKE_SOURCE_DIR}/llvm-project/libunwind"
    USES_TERMINAL_CONFIGURE ON
    USES_TERMINAL_BUILD     ON
    USES_TERMINAL_INSTALL   ON
	LOG_INSTALL ON
	BUILD_ALWAYS            ${REBUILD_TOOLCHAIN}
)


ExternalProject_Add(
    "libc++abi"
    DEPENDS "compiler-rt"
    DEPENDS "libunwind"
    CMAKE_CACHE_ARGS
		"-DCMAKE_BUILD_TYPE:STRING=Release"
		"-DCMAKE_TOOLCHAIN_FILE:STRING=${TOOLCHAIN_FILE}"
		"-DCMAKE_INSTALL_PREFIX:STRING=${SYSROOT_DIR}/usr"
        "-DCMAKE_C_FLAGS:STRING=${CMAKE_C_FLAGS} -nostdlib"
        "-DCMAKE_CXX_FLAGS:STRING=${CMAKE_CXX_FLAGS} -nostdlib"
		"-DLIBCXXABI_STATIC_LIBRARIES:STRING=unwind"
        "-DLIBCXXABI_ENABLE_THREADS:STRING=Off"
        "-DLIBCXXABI_USE_COMPILER_RT:STRING=On"
        "-DLIBCXXABI_ENABLE_SHARED:STRING=ON"
        "-DLIBCXXABI_ENABLE_STATIC_UNWINDER:STRING=ON"
        "-DLIBCXXABI_USE_LLVM_UNWINDER:STRING=OFF"
        "-DLIBUNWIND_ENABLE_SHARED:STRING=Off"
    SOURCE_DIR
        "${CMAKE_SOURCE_DIR}/llvm-project/libcxxabi"
    USES_TERMINAL_CONFIGURE ON
    USES_TERMINAL_BUILD     ON
    USES_TERMINAL_INSTALL   ON
	LOG_INSTALL ON
	BUILD_ALWAYS            ${REBUILD_TOOLCHAIN}
)



ExternalProject_Add(
    "libc++"
    DEPENDS "libc++abi"
    DEPENDS "libunwind"
    CMAKE_CACHE_ARGS
		"-DCMAKE_BUILD_TYPE:STRING=Release"
		"-DCMAKE_TOOLCHAIN_FILE:STRING=${TOOLCHAIN_FILE}"
		"-DCMAKE_INSTALL_PREFIX:STRING=${SYSROOT_DIR}/usr"
        "-DCMAKE_C_FLAGS:STRING=${CMAKE_C_FLAGS} -nostdlib"
        "-DCMAKE_CXX_FLAGS:STRING=${CMAKE_CXX_FLAGS} -nostdlib"
        "-DLIBCXX_ENABLE_THREADS:STRING=On"
        "-DLIBCXX_ENABLE_MONOTONIC_CLOCK:STRING=On"
        "-DLIBCXX_ENABLE_FILESYSTEM:STRING=Off"
        "-DLIBCXX_ENABLE_STATIC_ABI_LIBRARY:STRING=On"
        "-DLIBCXX_ENABLE_SHARED:STRING=ON"
        "-DLIBCXX_USE_COMPILER_RT:STRING=On"
        "-DLIBCXX_CXX_ABI:STRING=libcxxabi"
        "-DLIBCXX_CXX_STATIC_ABI_LIBRARY:STRING=c++abi"
		"-DLIBCXX_CXX_ABI_LIBRARY_PATH:STRING=${SYSROOT_DIR}/usr/lib"
        "-DLIBCXX_INCLUDE_TESTS:STRING=Off"
		"-DLIBCXX_HAS_MUSL_LIBC:STRING=ON"
    SOURCE_DIR
        "${CMAKE_SOURCE_DIR}/llvm-project/libcxx"
    USES_TERMINAL_CONFIGURE ON
    USES_TERMINAL_BUILD     ON
    USES_TERMINAL_INSTALL   ON
	LOG_INSTALL ON
	BUILD_ALWAYS            ${REBUILD_TOOLCHAIN}
)

if(ON)
ExternalProject_Add(
	"compiler-rt-phase-2"
	DEPENDS "llvm"
	DEPENDS "musl-headers"
	DEPENDS "compiler-rt"
	DEPENDS "libc++"
	CMAKE_CACHE_ARGS
	"-DCMAKE_BUILD_TYPE:STRING=Release"
	"-DCMAKE_TOOLCHAIN_FILE:STRING=${TOOLCHAIN_FILE}"
	"-DCMAKE_C_FLAGS:STRING=${CMAKE_C_FLAGS} -nostdlib"
	"-DCMAKE_CXX_FLAGS:STRING=${CMAKE_CXX_FLAGS} -nostdlib"
	"-DCOMPILER_RT_DEFAULT_TARGET_ONLY:BOOL=ON"
	"-DCOMPILER_RT_ENABLE_SHARED:STRING=OFF"
	"-DCOMPILER_RT_USE_LIBCXX:STRING=ON"
	"-DCOMPILER_RT_CRT_USE_EH_FRAME_REGISTRY:STRING=OFF"
	"-DCOMPILER_RT_EXCLUDE_ATOMIC_BUILTIN:STRING=OFF"
	"-DCOMPILER_RT_SANITIZERS_TO_BUILD:STRING="
	CMAKE_ARGS
	"-DCMAKE_INSTALL_PREFIX:STRING=${TOOLCHAIN_DIR}/lib/clang/12.0.0"
	SOURCE_DIR
	"${CMAKE_SOURCE_DIR}/llvm-project/compiler-rt"
	USES_TERMINAL_CONFIGURE ON
	USES_TERMINAL_BUILD ON
	USES_TERMINAL_INSTALL ON
	LOG_INSTALL ON
	BUILD_ALWAYS ${REBUILD_TOOLCHAIN}
	)
endif()

ExternalProject_Add(
	"utils"
	DEPENDS
	"twz-bootstrap"
	"libc++"
	CMAKE_CACHE_ARGS
	"-DCMAKE_INSTALL_PREFIX:STRING=${TOOLCHAIN_DIR}"
	"-DCMAKE_BUILD_TYPE:STRING=Release"
	SOURCE_DIR "${CMAKE_SOURCE_DIR}/utils"
	USES_TERMINAL_CONFIGURE ON
	USES_TERMINAL_BUILD ON
	USES_TERMINAL_INSTALL ON
	LOG_INSTALL ON
	BUILD_ALWAYS ${REBUILD_TOOLCHAIN}
)

ExternalProject_Add(
	"rustc"
	DEPENDS
	"twz-bootstrap"
	"twix-bootstrap"
	"musl"
	"libunwind"
	"libc++"
	"compiler-rt-phase-2"
	SOURCE_DIR "${CMAKE_SOURCE_DIR}/rust"
	CONFIGURE_COMMAND
	  "sed"
	  "-e" "s|@sysroot@|${SYSROOT_DIR}|g"
	  "-e" "s|@toolchain@|${TOOLCHAIN_DIR}|g"
	  "-e" "s|@target@|${TWIZZLER_TRIPLE}|g"
	  "${CMAKE_SOURCE_DIR}/rust-config.toml"
	  ">" "${CMAKE_BINARY_DIR}/rust/config.toml"
	  BINARY_DIR "${CMAKE_BINARY_DIR}/rust"
	  BUILD_COMMAND
	  "${CMAKE_SOURCE_DIR}/rust/x.py" "build" "-j" "${BUILD_CORES}"
	  INSTALL_COMMAND
	  "${CMAKE_SOURCE_DIR}/rust/x.py" "install" "-j" "${BUILD_CORES}"
	  USES_TERMINAL_CONFIGURE ON
	  USES_TERMINAL_BUILD ON
	  USES_TERMINAL_INSTALL ON
	  BUILD_ALWAYS ${REBUILD_TOOLCHAIN}
)

