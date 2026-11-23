# Configure checks for config.h, following configure.in.in, the
# subdirectories' configure.in.in and admin/acinclude.m4.in.  Only what the
# sources actually test is checked.

include(CheckIncludeFile)
include(CheckIncludeFiles)
include(CheckSymbolExists)
include(CheckFunctionExists)
include(CheckLibraryExists)
include(CheckStructHasMember)
include(CheckTypeSize)
include(CheckCSourceCompiles)
include(CheckCXXSourceCompiles)
include(CheckCSourceRuns)
include(TestBigEndian)
include(CMakePushCheckState)

set(KDE_DISTRIBUTION_TEXT "compiled sources" CACHE STRING
  "Distribution text appended to the OS in bug reports")

# KDE_COMPILER_VERSION: first line of "$CXX --version"
execute_process(COMMAND ${CMAKE_CXX_COMPILER} --version
  OUTPUT_VARIABLE _ver ERROR_QUIET)
string(REGEX REPLACE "\n.*" "" KDE_COMPILER_VERSION "${_ver}")
string(REPLACE "\\" "\\\\" KDE_COMPILER_VERSION "${KDE_COMPILER_VERSION}")
string(REPLACE "\"" "\\\"" KDE_COMPILER_VERSION "${KDE_COMPILER_VERSION}")
if(NOT KDE_COMPILER_VERSION)
  set(KDE_COMPILER_VERSION unknown)
endif()
# KDE_COMPILING_OS: uname -smr
set(KDE_COMPILING_OS "${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_PROCESSOR} ${CMAKE_SYSTEM_VERSION}")
execute_process(COMMAND uname -smr OUTPUT_VARIABLE _uname
  OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET RESULT_VARIABLE _r)
if(_r EQUAL 0 AND _uname)
  set(KDE_COMPILING_OS "${_uname}")
endif()

# ---- GNU extensions (AC_CHECK_GNU_EXTENSIONS) -------------------------
check_symbol_exists(__GNU_LIBRARY__ features.h _GNU_SOURCE)

# ---- byte order, sizes -------------------------------------------------
test_big_endian(WORDS_BIGENDIAN)
check_type_size("char *" SIZEOF_CHAR_P)
check_type_size(int SIZEOF_INT)
check_type_size(long SIZEOF_LONG)
check_type_size(short SIZEOF_SHORT)
check_type_size(size_t SIZEOF_SIZE_T)
check_type_size("unsigned long" SIZEOF_UNSIGNED_LONG)

# ---- headers ------------------------------------------------------------
foreach(_h
    alloca.h crt_externs.h dlfcn.h float.h fstab.h ieeefp.h libutil.h
    limits.h malloc.h mntent.h paths.h pty.h stdint.h stdio.h stdlib.h
    strings.h string.h sysent.h sys/bitypes.h sys/filio.h sys/mman.h
    sys/mntent.h sys/mnttab.h sys/mount.h sys/param.h sys/select.h
    sys/stat.h sys/stropts.h sys/time.h sys/types.h sys/ucred.h
    termios.h termio.h unistd.h util.h valgrind/memcheck.h)
  string(TOUPPER "HAVE_${_h}" _var)
  string(REGEX REPLACE "[/.]" "_" _var "${_var}")
  check_include_file("${_h}" ${_var})
endforeach()
check_include_files("sys/types.h;arpa/nameser8_compat.h" HAVE_ARPA_NAMESER8_COMPAT_H)
check_include_files("sys/types.h;sys/socket.h;netdb.h;netinet/in.h" HAVE_NETINET_IN_H)
check_include_files("sys/types.h;sys/socket.h;netdb.h;net/if.h" HAVE_NET_IF_H)

cmake_push_check_state(RESET)
set(CMAKE_REQUIRED_INCLUDES ${X11_INCLUDE_DIR})
check_include_files("X11/Xlib.h;X11/Xutil.h;X11/extensions/shape.h" HAVE_X11_EXTENSIONS_SHAPE_H)
check_include_files("X11/Xlib.h;X11/extensions/XShm.h" HAVE_X11_EXTENSIONS_XSHM_H)
check_include_file(X11/ICE/ICElib.h HAVE_X11_ICE_ICELIB_H)
cmake_pop_check_state()

# AC_HEADER_TIME
check_c_source_compiles("
#include <sys/types.h>
#include <sys/time.h>
#include <time.h>
int main(void) { struct tm *tp = 0; return tp != 0; }" TIME_WITH_SYS_TIME)

check_symbol_exists(S_ISSOCK sys/stat.h HAVE_S_ISSOCK)

# ---- functions ----------------------------------------------------------
foreach(_f
    strtoll socket seteuid setegid strfmon stpcpy gettimeofday readdir_r
    setpriority usleep random setenv unsetenv mkstemps mkstemp mkdtemp
    initgroups setgroups getgroups grantpt getpt unlockpt ptsname _getpty
    getpeereid inet_ntop inet_pton getpeername getsockname getsockopt
    gethostbyname2_r gethostbyname_r gethostbyname2 if_nametoindex
    getprotobyname_r getservbyname_r getservbyport_r poll madvise
    getaddrinfo freeaddrinfo getnameinfo gai_strerror setmntent getmntinfo
    setfsent mmap munmap getpagesize snprintf vsnprintf strlcpy strlcat
    posix_fadvise backtrace)
  # Link tests without headers, like AC_CHECK_FUNCS.  Functions that
  # may be macros or are checked with their headers in autotools are
  # handled below (on glibc only res_init differs: it is a macro for
  # __res_init, and libc exports no res_init symbol).
  string(TOUPPER "HAVE_${_f}" _var)
  check_function_exists(${_f} ${_var})
endforeach()
set(HAVE_FADVISE ${HAVE_POSIX_FADVISE})

# AC_CHECK_RES_INIT: res_init() with <resolv.h>, with -lresolv if needed
# (glibc < 2.34 and some other systems keep it in libresolv).
set(_res_init_headers "sys/types.h;netinet/in.h;arpa/nameser.h;resolv.h")
check_symbol_exists(res_init "${_res_init_headers}" HAVE_RES_INIT)
set(KDE3_LIBRESOLV "")
if(NOT HAVE_RES_INIT)
  cmake_push_check_state(RESET)
  set(CMAKE_REQUIRED_LIBRARIES resolv)
  check_symbol_exists(res_init "${_res_init_headers}" HAVE_RES_INIT_IN_LIBRESOLV)
  cmake_pop_check_state()
  if(HAVE_RES_INIT_IN_LIBRESOLV)
    set(HAVE_RES_INIT 1)
    set(KDE3_LIBRESOLV resolv)
  endif()
endif()

# kdecore: "revoke(tty) in unistd.h" (KPty calls it on BSD only).
# glibc has a stub that links but always fails with ENOSYS; it marks
# such functions with __stub_<name> in <gnu/stubs.h> (from <unistd.h>),
# which AC_CHECK_FUNC honours.  Do not count the stub.
check_c_source_compiles("
#include <unistd.h>
#if defined(__stub_revoke) || defined(__stub___revoke)
#error revoke() is a stub
#endif
int main(void) { return revoke(\"/dev/tty\"); }" HAVE_REVOKE)

if(HAVE_GETSERVBYNAME_R)
  check_symbol_exists(getservbyname_r "sys/types.h;sys/socket.h;netdb.h"
    HAVE_DECL_GETSERVBYNAME_R)
endif()
if(HAVE_GETADDRINFO AND NOT (HAVE_FREEADDRINFO AND HAVE_GETNAMEINFO AND HAVE_GAI_STRERROR))
  set(HAVE_BROKEN_GETADDRINFO 1)
endif()

# openpty (kdecore: AC_TRY_RUN with $LIBUTIL; kdesu: -lutil)
check_function_exists(openpty HAVE_OPENPTY)
if(NOT HAVE_OPENPTY)
  check_library_exists(util openpty "" HAVE_OPENPTY_IN_LIBUTIL)
  if(HAVE_OPENPTY_IN_LIBUTIL)
    set(HAVE_OPENPTY 1)
    set(KDE3_LIBUTIL util)
  endif()
endif()

# kjs: finite/_finite in -lm, isnan/isinf from <math.h>
cmake_push_check_state(RESET)
set(CMAKE_REQUIRED_LIBRARIES m)
check_function_exists(finite HAVE_FUNC_FINITE)
check_function_exists(_finite HAVE_FUNC__FINITE)
check_c_source_compiles("#include <math.h>
float f; int main(void) { return isnan(f); }" HAVE_FUNC_ISNAN)
check_c_source_compiles("#include <math.h>
float f; int main(void) { return isinf(f); }" HAVE_FUNC_ISINF)
cmake_pop_check_state()

# kdesu
check_c_source_compiles("#include <sched.h>
int main(void) { sched_getscheduler(0); return 0; }" POSIX1B_SCHEDULING)
check_cxx_source_compiles("#include <sys/socket.h>
int main() { struct ucred cred; (void)cred; return 0; }" HAVE_STRUCT_UCRED)

# ---- types and members -------------------------------------------------
cmake_push_check_state(RESET)
set(CMAKE_EXTRA_INCLUDE_FILES sys/types.h sys/socket.h netdb.h)
check_type_size("struct addrinfo" STRUCT_ADDRINFO)
set(CMAKE_EXTRA_INCLUDE_FILES sys/types.h netinet/in.h)
check_type_size("struct sockaddr_in6" STRUCT_SOCKADDR_IN6)
cmake_pop_check_state()
if(HAVE_STRUCT_ADDRINFO)
  set(HAVE_STRUCT_ADDRINFO 1)
endif()
if(HAVE_STRUCT_SOCKADDR_IN6)
  set(HAVE_STRUCT_SOCKADDR_IN6 1)
  check_struct_has_member("struct sockaddr_in6" sin6_scope_id netinet/in.h
    HAVE_STRUCT_SOCKADDR_IN6_SIN6_SCOPE_ID)
endif()
check_struct_has_member("struct sockaddr" sa_len sys/socket.h
  HAVE_STRUCT_SOCKADDR_SA_LEN)

check_cxx_source_compiles("#include <sys/types.h>
#include <sys/statvfs.h>
int main() { int flags = 0; struct statvfs *mntbufp; return getmntinfo(&mntbufp, flags); }"
  GETMNTINFO_USES_STATVFS)

# ---- mount table, paths -------------------------------------------------
foreach(_f /etc/mtab /etc/mnttab)
  if(EXISTS "${_f}")
    set(MTAB_FILE "${_f}")
    break()
  endif()
endforeach()

if(NOT X11_RGBFILE)
  set(_rgb /usr/share/X11/rgb.txt)
  foreach(_f /usr/share/X11/rgb.txt /usr/lib/X11/rgb.txt)
    if(EXISTS "${_f}")
      set(_rgb "${_f}")
      break()
    endif()
  endforeach()
  set(X11_RGBFILE "${_rgb}" CACHE FILEPATH "Location of rgb.txt")
endif()

set(ASPELL_DATADIR "/usr/lib/aspell" CACHE PATH
  "Where the aspell dictionaries live (KSpellConfig)")

# kdesu
find_program(__PATH_SU su)
find_program(__PATH_SUDO sudo)
mark_as_advanced(__PATH_SU __PATH_SUDO)
option(KDE3_KDESU_SUDO_BACKEND "Use sudo instead of su as kdesu backend" OFF)
if(KDE3_KDESU_SUDO_BACKEND)
  if(NOT __PATH_SUDO)
    message(FATAL_ERROR "sudo was chosen as kdesu backend, but was not found")
  endif()
  set(DEFAULT_SUPER_USER_COMMAND sudo)
else()
  set(DEFAULT_SUPER_USER_COMMAND su)
endif()

# ---- features ------------------------------------------------------------
option(KDE3_ENABLE_SENDFILE "Use sendfile() in kio_file (Linux only)" OFF)
if(KDE3_ENABLE_SENDFILE)
  check_include_file(sys/sendfile.h HAVE_SENDFILE)
endif()
option(KDE3_ENABLE_MITSHM "Use MIT-SHM for pixmap loading/saving" ON)
if(KDE3_ENABLE_MITSHM AND HAVE_X11_EXTENSIONS_XSHM_H)
  set(HAVE_MITSHM 1)
endif()

cmake_push_check_state(RESET)
set(CMAKE_REQUIRED_LIBRARIES ${X11_ICE_LIB})
check_function_exists(_IceTransNoListen HAVE__ICETRANSNOLISTEN)
cmake_pop_check_state()

# POSIX ACLs
find_package(PkgConfig REQUIRED)
option(KDE3_WITH_ACL "Support for POSIX ACLs" ON)
if(KDE3_WITH_ACL)
  check_include_file(sys/acl.h _have_sys_acl_h)
  check_include_file(sys/xattr.h HAVE_SYS_XATTR_H)
  if(CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
    check_library_exists(c acl_init "" _have_acl_lib)
    set(ACL_LIBS "")
  else()
    check_include_file(attr/libattr.h _have_attr_libattr_h)
    find_library(ACL_LIBRARY acl)
    find_library(ATTR_LIBRARY attr)
    set(_have_acl_lib FALSE)
    if(ACL_LIBRARY AND ATTR_LIBRARY AND _have_attr_libattr_h AND HAVE_SYS_XATTR_H)
      set(_have_acl_lib TRUE)
    endif()
    set(ACL_LIBS ${ACL_LIBRARY} ${ATTR_LIBRARY})
    check_include_file(acl/libacl.h _have_acl_libacl_h)
  endif()
  if(_have_sys_acl_h AND _have_acl_lib)
    set(USE_POSIX_ACL 1)
    if(_have_acl_libacl_h)
      set(HAVE_NON_POSIX_ACL_EXTENSIONS 1)
    endif()
  else()
    set(ACL_LIBS "")
  endif()
endif()

# utempter
# KPty registers its sessions in utmp through libutempter's
# utempter_add_record()/utempter_remove_record() (the library runs its
# setgid helper, wherever the distribution installed it).  Without the
# library API, KPty runs the helper program itself: UTEMPTER_HELPER.
option(KDE3_WITH_UTEMPTER "Use utempter for utmp management" ON)
set(KDE3_LIBUTEMPTER "")
if(KDE3_WITH_UTEMPTER)
  find_path(UTEMPTER_INCLUDE_DIR utempter.h)
  find_library(UTEMPTER_LIBRARY utempter)
  mark_as_advanced(UTEMPTER_INCLUDE_DIR UTEMPTER_LIBRARY)
  if(UTEMPTER_INCLUDE_DIR AND UTEMPTER_LIBRARY)
    cmake_push_check_state(RESET)
    set(CMAKE_REQUIRED_INCLUDES ${UTEMPTER_INCLUDE_DIR})
    set(CMAKE_REQUIRED_LIBRARIES ${UTEMPTER_LIBRARY})
    check_symbol_exists(utempter_add_record utempter.h HAVE_UTEMPTER_RECORD)
    cmake_pop_check_state()
  endif()
  if(HAVE_UTEMPTER_RECORD)
    set(HAVE_UTEMPTER 1)
    set(KDE3_LIBUTEMPTER ${UTEMPTER_LIBRARY})
  else()
    find_program(UTEMPTER_HELPER utempter
      PATHS /usr/libexec/utempter /usr/lib/utempter /usr/lib64/utempter
            /usr/libexec /usr/sbin
      NO_DEFAULT_PATH)
    mark_as_advanced(UTEMPTER_HELPER)
    if(UTEMPTER_HELPER)
      set(HAVE_UTEMPTER 1)
      # libutempter's helper (in a utempter/ directory, with the
      # strings "add" and "del") is run as "utempter add [<host>]" and
      # "utempter del"; the old one of Red Hat's utempter package as
      # "utempter -a <tty> <host>" and "utempter -d <tty>".
      # The helper may not be readable (setgid, mode 2711): look into it
      # only if it is, which CMake < 3.29 cannot tell.
      if(UTEMPTER_HELPER MATCHES "/utempter/utempter$")
        set(UTEMPTER_HELPER_ADD_DEL 1)
      elseif(CMAKE_VERSION VERSION_GREATER_EQUAL 3.29)
        if(IS_READABLE "${UTEMPTER_HELPER}")
          file(STRINGS "${UTEMPTER_HELPER}" _kde3_utempter_words
            LENGTH_MINIMUM 3 REGEX "^(add|del)$")
          if("add" IN_LIST _kde3_utempter_words
             AND "del" IN_LIST _kde3_utempter_words)
            set(UTEMPTER_HELPER_ADD_DEL 1)
          endif()
          unset(_kde3_utempter_words)
        endif()
      endif()
    endif()
  endif()
endif()

# libidn (IDNA)
option(KDE3_WITH_LIBIDN "Support for libidn (IDNA)" ON)
if(KDE3_WITH_LIBIDN)
  pkg_check_modules(LIBIDN IMPORTED_TARGET libidn)
  if(LIBIDN_FOUND)
    set(HAVE_LIBIDN 1)
    cmake_push_check_state(RESET)
    set(CMAKE_REQUIRED_INCLUDES ${LIBIDN_INCLUDE_DIRS})
    check_include_file(idna.h HAVE_IDNA_H)
    check_include_file(punycode.h HAVE_PUNYCODE_H)
    check_include_file(stringprep.h HAVE_STRINGPREP_H)
    cmake_pop_check_state()
  endif()
endif()

# libpsl (kssl: no wildcard certificate for a whole public suffix,
# "*.co.uk")
option(KDE3_WITH_LIBPSL "Refuse wildcard certificates for public suffixes (libpsl)" ON)
if(KDE3_WITH_LIBPSL)
  pkg_check_modules(LIBPSL IMPORTED_TARGET libpsl)
  if(LIBPSL_FOUND)
    set(HAVE_LIBPSL 1)
  endif()
endif()

# libsecret (kwalletd: wallets kept in the freedesktop.org Secret Service,
# e.g. the GNOME login keyring that the display manager unlocks)
option(KDE3_WITH_LIBSECRET "KWallet backend on the freedesktop.org Secret Service (libsecret)" ON)
option(KDE3_BUILD_KWALLETD_TESTS "Build the tests of the Secret Service backend of kwalletd (kio/misc/kwalletd/tests)" OFF)
if(KDE3_WITH_LIBSECRET)
  pkg_check_modules(LIBSECRET IMPORTED_TARGET libsecret-1)
  if(LIBSECRET_FOUND)
    set(HAVE_LIBSECRET 1)
  endif()
endif()

# libart (svgicons): not ported, libart_lgpl is not available any more
set(HAVE_LIBART "")

# SVG icons (KIconLoader) and images (kimgio), rendered with librsvg into
# a cairo surface
option(KDE3_WITH_SVG "SVG and SVGZ icons and images, rendered with librsvg" ON)
if(KDE3_WITH_SVG)
  pkg_check_modules(LIBRSVG IMPORTED_TARGET "librsvg-2.0 >= 2.52" cairo)
  if(LIBRSVG_FOUND)
    set(HAVE_LIBRSVG 1)
  endif()
endif()

# XRender (kdefx)
if(X11_Xrender_FOUND)
  set(HAVE_XRENDER 1)
endif()

# IA32 / AltiVec assembler support (kdefx)
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^i.86$")
  check_c_source_compiles("int main(void) { __asm__(\"pxor %mm0, %mm0\"); return 0; }" HAVE_X86_MMX)
  check_c_source_compiles("int main(void) { __asm__(\"xorps %xmm0, %xmm0\"); return 0; }" HAVE_X86_SSE)
  check_c_source_compiles("int main(void) { __asm__(\"xorpd %xmm0, %xmm0\"); return 0; }" HAVE_X86_SSE2)
  check_c_source_compiles("int main(void) { __asm__(\"femms\"); return 0; }" HAVE_X86_3DNOW)
elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "powerpc")
  check_c_source_compiles("int main(void) { __asm__(\"mtspr 256, %0\\n\\t\" \"vand %%v0, %%v0, %%v0\" : : \"r\"(-1) ); return 0; }" HAVE_PPC_ALTIVEC)
endif()

# kio: FAM, volmgt, dnotify, inotify
option(KDE3_WITH_FAM "Use libfam (File Alteration Monitor)" ON)
if(KDE3_WITH_FAM)
  find_library(FAM_LIBRARY fam)
  find_path(FAM_INCLUDE_DIR fam.h)
  if(FAM_LIBRARY AND FAM_INCLUDE_DIR)
    set(HAVE_FAM 1)
  endif()
endif()
check_library_exists(volmgt volmgt_running "" HAVE_VOLMGT)
option(KDE3_ENABLE_DNOTIFY "Use Linux directory notifications (dnotify)" OFF)
if(KDE3_ENABLE_DNOTIFY)
  check_c_source_compiles("
#include <fcntl.h>
#include <signal.h>
int main(void) {
#ifndef F_NOTIFY
#error no dir notification
#endif
  int fd = 0; siginfo_t *t = 0; (void)t;
  fcntl(fd, F_SETSIG, SIGRTMIN);
  fcntl(fd, F_NOTIFY, DN_DELETE|DN_CREATE|DN_MULTISHOT);
  return 0; }" HAVE_DNOTIFY)
endif()
option(KDE3_ENABLE_INOTIFY "Use Linux inode notifications (inotify)" ON)
if(KDE3_ENABLE_INOTIFY)
  # What kio/kio/kdirwatch.cpp uses: the C library's <sys/inotify.h>.  Not
  # __NR_inotify_init, which the asm-generic architectures (aarch64,
  # riscv64, loongarch64) lack; they only have inotify_init1.
  check_symbol_exists(inotify_init "sys/inotify.h" HAVE_INOTIFY)
endif()

# Image and compression libraries (AC_FIND_ZLIB/JPEG/PNG/TIFF/BZIP2)
# 1.2.4: inflateReset2 (kio/httpfilter)
find_package(ZLIB 1.2.4 REQUIRED)
set(HAVE_LIBZ 1)
# libkhtml always links libjpeg (khtml/misc), kde3-qt libpng
find_package(JPEG REQUIRED)
set(HAVE_LIBJPEG 1)
find_package(PNG REQUIRED)
set(HAVE_LIBPNG 1)
option(KDE3_WITH_TIFF "Enable tiff support" ON)
if(KDE3_WITH_TIFF)
  find_package(TIFF)
  if(TIFF_FOUND)
    set(HAVE_LIBTIFF 1)
  endif()
endif()
# kimgio: WebP images (new, no autotools equivalent)
option(KDE3_WITH_WEBP "WebP image support (kimgio, needs libwebp)" ON)
if(KDE3_WITH_WEBP)
  pkg_check_modules(LIBWEBP IMPORTED_TARGET "libwebp >= 0.5")
  if(NOT LIBWEBP_FOUND)
    message(FATAL_ERROR "libwebp (0.5 or later) was not found; it is needed for the WebP "
      "image plugin.  Install it, or configure with -DKDE3_WITH_WEBP=OFF to "
      "build without WebP support.")
  endif()
  set(HAVE_LIBWEBP 1)
endif()
# kimgio: HEIF images (new, no autotools equivalent)
option(KDE3_WITH_HEIF "HEIF/HEIC image support (kimgio, needs libheif)" ON)
if(KDE3_WITH_HEIF)
  pkg_check_modules(LIBHEIF IMPORTED_TARGET "libheif >= 1.20")
  if(NOT LIBHEIF_FOUND)
    message(FATAL_ERROR "libheif (1.20 or later) was not found; it is needed for the HEIF "
      "image plugin.  Install it, or configure with -DKDE3_WITH_HEIF=OFF to "
      "build without HEIF support.")
  endif()
  set(HAVE_LIBHEIF 1)
endif()
# kimgio: AVIF images (new, no autotools equivalent)
option(KDE3_WITH_AVIF "AVIF image support (kimgio, needs libavif)" ON)
if(KDE3_WITH_AVIF)
  pkg_check_modules(LIBAVIF IMPORTED_TARGET "libavif >= 1.2")
  if(NOT LIBAVIF_FOUND)
    message(FATAL_ERROR "libavif (1.2 or later) was not found; it is needed for the AVIF "
      "image plugin.  Install it, or configure with -DKDE3_WITH_AVIF=OFF to "
      "build without AVIF support.")
  endif()
  set(HAVE_LIBAVIF 1)
endif()
# kimgio: JPEG XL images (new, no autotools equivalent)
option(KDE3_WITH_JXL "JPEG XL image support (kimgio, needs libjxl)" ON)
if(KDE3_WITH_JXL)
  pkg_check_modules(LIBJXL IMPORTED_TARGET "libjxl >= 0.9" "libjxl_threads >= 0.9")
  if(NOT LIBJXL_FOUND)
    message(FATAL_ERROR "libjxl (0.9 or later, with libjxl_threads) was not found; it is "
      "needed for the JPEG XL image plugin.  Install it, or configure with "
      "-DKDE3_WITH_JXL=OFF to build without JPEG XL support.")
  endif()
  set(HAVE_LIBJXL 1)
endif()
# libkdoctools/meinproc always link libbz2 (index.cache.bz2)
find_package(BZip2 REQUIRED)
if(BZIP2_FOUND)
  set(HAVE_BZIP2_SUPPORT 1)
  if(BZIP2_NEED_PREFIX)
    set(NEED_BZ2_PREFIX 1)
  endif()
endif()
# KFilterDev/KTar: xz (and legacy lzma) and Zstandard compression filters.
# Required: most tar archives (KDE Store, distributions) use one of them.
# Fedora: xz-devel, libzstd-devel; Arch: xz, zstd.
pkg_check_modules(LIBLZMA REQUIRED IMPORTED_TARGET "liblzma >= 5.0")
pkg_check_modules(LIBZSTD REQUIRED IMPORTED_TARGET "libzstd >= 1.4.0")
# KFilterDev/KTar: LZ4 frame compression filter (Fedora: lz4-devel; Arch: lz4)
option(KDE3_WITH_LZ4 "LZ4 compression filter (KFilterDev, needs liblz4)" ON)
if(KDE3_WITH_LZ4)
  pkg_check_modules(LIBLZ4 IMPORTED_TARGET "liblz4 >= 1.9.0")
  if(NOT LIBLZ4_FOUND)
    message(FATAL_ERROR "liblz4 (1.9 or later) was not found; it is needed for the LZ4 "
      "compression filter.  Install it, or configure with -DKDE3_WITH_LZ4=OFF to "
      "build without LZ4 support.")
  endif()
  set(HAVE_LIBLZ4 1)
endif()
# kio_http: Brotli content encoding (Fedora: brotli-devel; Arch: brotli)
option(KDE3_WITH_BROTLI "Brotli HTTP content encoding (kio_http, needs libbrotlidec)" ON)
if(KDE3_WITH_BROTLI)
  pkg_check_modules(LIBBROTLIDEC IMPORTED_TARGET "libbrotlidec >= 1.0")
  if(NOT LIBBROTLIDEC_FOUND)
    message(FATAL_ERROR "libbrotlidec (1.0 or later) was not found; it is needed for the "
      "Brotli content encoding of kio_http.  Install it, or configure with "
      "-DKDE3_WITH_BROTLI=OFF to build without Brotli support.")
  endif()
  set(HAVE_LIBBROTLIDEC 1)
endif()

# kjs: PCRE (libpcre + libpcreposix)
option(KDE3_WITH_PCRE "Use libpcre for JavaScript regular expressions" ON)
if(KDE3_WITH_PCRE)
  pkg_check_modules(PCRE REQUIRED IMPORTED_TARGET libpcre libpcreposix)
  set(HAVE_PCREPOSIX 1)
endif()

# khtml: libthai (autotools default: no), valgrind header checked above
option(KDE3_WITH_LIBTHAI "Thai-language line breaking with libthai" OFF)
if(KDE3_WITH_LIBTHAI)
  pkg_check_modules(LIBTHAI REQUIRED IMPORTED_TARGET libthai)
  set(HAVE_LIBTHAI 1)
endif()

# kate: Lua 5.0 indentation scripts.  The code uses the Lua 5.0 API,
# current Lua is not compatible, so it is off by default.
option(KDE3_WITH_LUA "Lua 5.0 indentation scripts in Kate" OFF)
if(KDE3_WITH_LUA)
  set(HAVE_LUA 1)
endif()

# kioslave/http: GSSAPI
option(KDE3_WITH_GSSAPI "GSSAPI (Kerberos) authentication in kio_http" ON)
if(KDE3_WITH_GSSAPI)
  pkg_check_modules(GSSAPI IMPORTED_TARGET krb5-gssapi)
  if(GSSAPI_FOUND)
    set(HAVE_LIBGSSAPI 1)
    # autotools: MIT if "krb5-config --vendor" names Massachusetts
    find_program(KRB5_CONFIG_EXECUTABLE krb5-config)
    mark_as_advanced(KRB5_CONFIG_EXECUTABLE)
    if(KRB5_CONFIG_EXECUTABLE)
      execute_process(COMMAND ${KRB5_CONFIG_EXECUTABLE} --vendor
        OUTPUT_VARIABLE _krb5_vendor ERROR_QUIET)
      if(_krb5_vendor MATCHES "Massachusetts")
        set(GSSAPI_MIT 1)
      endif()
    endif()
  endif()
endif()

# dnssd
option(KDE3_WITH_DNSSD "DNS-SD (Zeroconf) support through libdns_sd" ON)
if(KDE3_WITH_DNSSD)
  find_path(DNSSD_INCLUDE_DIR dns_sd.h)
  find_library(DNSSD_LIBRARY dns_sd)
  if(DNSSD_INCLUDE_DIR AND DNSSD_LIBRARY)
    set(HAVE_DNSSD 1)
  endif()
endif()

# kssl: libkio links OpenSSL (>= 3.0, Apache-2.0) directly.  SSL is
# not optional any more; KSSL_HAVE_SSL stays defined for the sources and
# the installed headers that test it.
find_package(OpenSSL 3.0 REQUIRED)
set(HAVE_SSL 1)
set(KSSL_HAVE_SSL 1)

# kdoctools: libxml2, libxslt (meinproc is needed to build the
# handbooks), xmllint
find_package(LibXml2 REQUIRED)
find_package(LibXslt REQUIRED)
find_program(XMLLINT_EXECUTABLE xmllint)
mark_as_advanced(XMLLINT_EXECUTABLE)
set(XMLLINT "${XMLLINT_EXECUTABLE}")

# kinit: Xft and fontconfig preinitialisation (kde3-qt is built with Xft)
find_package(Fontconfig)
find_package(Freetype)
if(X11_Xft_FOUND AND FREETYPE_FOUND)
  set(KDEINIT_USE_XFT 1)
endif()
if(Fontconfig_FOUND)
  set(KDEINIT_USE_FONTCONFIG 1)
endif()
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(KDEINIT_OOM_PROTECT 1)
endif()

# knotify: PulseAudio player (libpulse and libsndfile, optional)
option(KDE3_WITH_PULSEAUDIO "knotify plays sounds with libpulse and libsndfile" ON)
if(KDE3_WITH_PULSEAUDIO)
  pkg_check_modules(KNOTIFY_PULSE IMPORTED_TARGET "libpulse >= 0.9.16" "sndfile >= 1.0.21")
  if(KNOTIFY_PULSE_FOUND)
    set(HAVE_PULSEAUDIO 1)
  endif()
endif()

# kdeprint: CUPS is the only print system (kdeprint/configure.in.in)
find_package(Cups 1.1.9 REQUIRED)
set(HAVE_CUPS 1)
if(Cups_VERSION VERSION_GREATER_EQUAL 1.1.20)
  # CUPS doesn't have password caching
  set(HAVE_CUPS_NO_PWD_CACHE 1)
endif()
