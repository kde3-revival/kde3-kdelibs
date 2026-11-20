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

set(KDELIBSUFF "" CACHE STRING "Suffix of the library directories (lib<suffix>)")
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
    posix_fadvise res_init backtrace revoke)
  string(TOUPPER "HAVE_${_f}" _var)
  check_function_exists(${_f} ${_var})
endforeach()
set(HAVE_FADVISE ${HAVE_POSIX_FADVISE})
if(NOT HAVE_RES_INIT)
  check_library_exists(resolv __res_init "" HAVE_RES_INIT)
endif()
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
option(KDE3_WITH_UTEMPTER "Use utempter for utmp management" ON)
if(KDE3_WITH_UTEMPTER)
  find_library(UTEMPTER_LIBRARY utempter)
  if(UTEMPTER_LIBRARY)
    check_library_exists(utempter addToUtmp "" HAVE_UTEMPTER)
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

# libart (svgicons): not ported, libart_lgpl is not available any more
set(HAVE_LIBART "")

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
  check_c_source_compiles("
#include <asm/unistd.h>
#define _S390_BITOPS_H
#include <linux/inotify.h>
#ifndef IN_ALL_EVENTS
#error no inotify notification
#endif
#ifndef __NR_inotify_init
#error no __NR_inotify_init
#endif
int main(void) { return 0; }" HAVE_INOTIFY)
endif()

# Image and compression libraries (AC_FIND_ZLIB/JPEG/PNG/TIFF/BZIP2)
find_package(ZLIB REQUIRED)
set(HAVE_LIBZ 1)
find_package(JPEG)
if(JPEG_FOUND)
  set(HAVE_LIBJPEG 1)
endif()
find_package(PNG)
if(PNG_FOUND)
  set(HAVE_LIBPNG 1)
endif()
option(KDE3_WITH_TIFF "Enable tiff support" ON)
if(KDE3_WITH_TIFF)
  find_package(TIFF)
  if(TIFF_FOUND)
    set(HAVE_LIBTIFF 1)
  endif()
endif()
find_package(BZip2)
if(BZIP2_FOUND)
  set(HAVE_BZIP2_SUPPORT 1)
  if(BZIP2_NEED_PREFIX)
    set(NEED_BZ2_PREFIX 1)
  endif()
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

# kssl.  KOpenSSLProxy and the kssl classes are written against the
# OpenSSL 0.9.x API (public structs, STACK, ASN1_HEADER, macros that
# became functions and vice versa) and do not compile with OpenSSL 1.1
# or later yet.  Until they are ported, kdelibs is built without SSL
# support, as autotools did when OpenSSL was missing.
option(KDE3_WITH_OPENSSL "SSL support in kssl (needs the OpenSSL 3 port of kssl)" OFF)
if(KDE3_WITH_OPENSSL)
  find_package(OpenSSL REQUIRED)
  set(HAVE_SSL 1)
  set(KSSL_HAVE_SSL 1)
endif()

# kdoctools: libxml2, libxslt, xmllint
find_package(LibXml2)
find_package(LibXslt)
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
