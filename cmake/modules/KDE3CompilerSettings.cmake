#[=======================================================================[.rst:
KDE3CompilerSettings
--------------------

The compiler and linker settings of the KDE 3.5 autotools build
(admin/acinclude.m4.in: AC_CHECK_COMPILERS, KDE_CHECK_THREADING,
KDE_CHECK_CLOSURE) for GCC and Clang.  kdelibs includes it; other KDE 3
modules may include it after ``find_package(KDE3)`` to get the same
settings (it is not applied to them automatically).

Build types (autotools ``--enable-debug`` variants):

``RelWithDebInfo`` (default)
  ``-O2 -g``: autotools' default (``-O2``, no NDEBUG) plus symbols.
``Release``
  ``-O2 -DNDEBUG -DNO_DEBUG`` (``--disable-debug``).
``Debug``
  ``-g3 -fno-inline`` (``--enable-debug=full``).

Always: C++ ``-fno-exceptions -fno-check-new -fno-common`` (targets
whose Makefile.am used $(USE_EXCEPTIONS) get ``-fexceptions`` from the
KDE3 macros' ``EXCEPTIONS`` option), the warning flags of
``kde_use_warnings`` unless ``KDE3_ENABLE_WARNINGS`` is OFF, and
``-Wl,--no-undefined`` for shared libraries (libtool's -no-undefined
is a no-op on Linux; this only turns missing symbols, like a missing
moc file, into link errors).

Deliberately not taken over: ``-ansi -D_XOPEN_SOURCE=500 -D_BSD_SOURCE``
and C ``-std=iso9899:1990`` (the compilers' default standards and
glibc feature set are used), ``-fvisibility=hidden`` (autotools only
used it with --enable-gcc-hidden-visibility, default off).
#]=======================================================================]

include_guard(GLOBAL)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING
    "Build type: RelWithDebInfo (default), Release, Debug" FORCE)
endif()

foreach(_lang C CXX)
  set(CMAKE_${_lang}_FLAGS_RELEASE "-O2 -DNDEBUG -DNO_DEBUG")
  set(CMAKE_${_lang}_FLAGS_RELWITHDEBINFO "-O2 -g")
  set(CMAKE_${_lang}_FLAGS_DEBUG "-g3 -fno-inline")
endforeach()

option(KDE3_ENABLE_WARNINGS "Compile with the warning flags of KDE 3.5 autotools" ON)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(
    "$<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions;-fno-check-new;-fno-common>")
  if(KDE3_ENABLE_WARNINGS)
    add_compile_options(
      "$<$<COMPILE_LANGUAGE:CXX>:-Wall;-W;-Wpointer-arith;-Wundef;-Wno-long-long;-Wcast-align;-Wchar-subscripts;-Wformat-security;-Wmissing-format-attribute;-Wno-non-virtual-dtor>"
      "$<$<COMPILE_LANGUAGE:C>:-W;-Wall;-Wchar-subscripts;-Wshadow;-Wpointer-arith;-Wmissing-prototypes;-Wwrite-strings;-Wformat-security;-Wmissing-format-attribute>")
  endif()
  # Modules are left alone: kded modules, for example, resolve
  # KDEDModule from the kded process (LIB_KDED is empty on Linux).
  string(APPEND CMAKE_SHARED_LINKER_FLAGS " -Wl,--no-undefined")
endif()
