# kde3-libs

The KDE 3.5 core libraries (kdelibs 3.5.10), maintained by the
[kde3-revival](https://github.com/kde3-revival) project for today's Linux
systems.

Every KDE 3 program needs this package. It is built on
[kde3-qt](https://github.com/kde3-revival/kde3-qt) and is in turn the base
of [kde3-base](https://github.com/kde3-revival/kde3-base) (the desktop) and
[kde3-apps](https://github.com/kde3-revival/kde3-apps) (applications).

The first commit of this repository is the pristine kdelibs 3.5.10 release;
everything after it is this project's work, so `git log` and `git blame`
lead back to the original code. How the project works is described on the
[organisation page](https://github.com/kde3-revival).

## Contents

| Directory | What it is |
|---|---|
| `dcop` | DCOP, the inter-process communication of KDE 3, and its IDL tools |
| `kdecore` | Core non-GUI classes: application, configuration, locale, standard dirs, networking, icons |
| `kdeui` | Widgets, actions, dialogs, main windows, XML GUI |
| `kdefx` | Pixmap and image effects used by styles and widgets |
| `kio` | KIO: network-transparent file access, ksycoca (services and MIME types), KFile dialogs, SSL (kssl), KWallet client |
| `kioslave` | Basic I/O slaves: file, ftp, http/https, gzip, bzip2, xz, zstd, lz4, metainfo |
| `kparts` | KParts, the embeddable component model |
| `khtml`, `kjs` | The HTML/CSS engine and the JavaScript interpreter |
| `kinit` | kdeinit, klauncher and the startup helpers |
| `kded` | The KDE daemon and its module loader |
| `kwallet` | KWallet client library and backend |
| `knotify` | The notification daemon (sounds through PulseAudio) |
| `kdeprint` | The printing system, on CUPS |
| `kimgio` | Image format plugins for Qt's image I/O |
| `kstyles` | The widget styles (Plastik, Keramik, HighColor, ...) |
| `kate` | The Kate text editor component (KTextEditor part) |
| `kabc`, `kresources` | Address book library and the resource framework under it |
| `knewstuff` | "Get Hot New Stuff" downloads |
| `kutils`, `interfaces`, `kdesu`, `dnssd`, `kdewidgets`, `libkscreensaver` | Smaller libraries and plugins |
| `kdoctools` | meinproc and the DocBook stylesheets for the handbooks |
| `mimetypes`, `pics`, `licenses`, `doc` | MIME type definitions, icons, license texts, documentation |
| `cmake` | The CMake modules, installed for the other KDE 3 modules |

## Changes compared with KDE 3.5.10

### Build

- CMake replaces autotools, libtool and libltdl. The library names,
  SONAMEs, install layout and kdeinit modules are those of the autotools
  build.
- kdelibs installs a CMake package, `KDE3Libs`, with imported targets
  (`KDE3::kdecore`, `KDE3::kio`, ...) and the macros that replace the
  KDE 3 automake extensions (`kde3_add_library()`, `kde3_add_module()`,
  `kde3_add_kdeinit_executable()`, DCOP, uic and kconfig_compiler
  sources). Other modules use `find_package(KDE3Libs 3.5)`.
- Builds with current GCC, glibc and system libraries (C++ fixes for
  current compilers, CUPS 2 API, OpenSSL 3, libutempter API, glibc's
  inotify).
- Binaries get an `$ORIGIN`-relative RUNPATH; no `ld.so.conf` entry is
  needed.

### Security and TLS

- kssl links OpenSSL 3 directly; SSL is no longer optional.
- Uses the system's CA store, sends SNI, matches host names by RFC 6125,
  refuses wildcard certificates for public suffixes (libpsl), reads
  certificate dates after 2049, uses the system's cipher list.
- Fixes for archive path traversal (tar, zip), unchecked privilege drops
  in KProcess, KCrash and start_kdeinit, and several KJS/KHTML memory
  errors.
- `$(...)` in `[$e]` configuration entries is no longer run as a shell
  command (see `kdecore/README.kiosk`).
- TCP keepalive and a user timeout detect dead peers in KIO connections.

### Formats and data

- Compression: xz, lzma, zstd and lz4 filters for KFilterDev and KTar;
  kio_http decodes zstd, Brotli and zlib-wrapped deflate.
- kimgio reads WebP (also writes), HEIF/HEIC, AVIF, JPEG XL, SVG/SVGZ
  (librsvg) and ICO files with PNG entries; hardened against malformed
  files.
- KIconLoader renders SVG icons with librsvg.
- MIME types and magic rules from freedesktop.org shared-mime-info, in
  addition to KDE's own.

### Desktop integration

- knotify plays sounds through PulseAudio (libpulse and libsndfile).
- KWallet can keep its wallets in the freedesktop.org Secret Service (for
  example the GNOME keyring), through libsecret.
- kinit starts the entries of the XDG autostart directories;
  `autostartrc` can skip entries by name.
- KStandardDirs handles the XDG data and config directories correctly.
- knewstuff gets new content from the KDE Store (Open Collaboration
  Services) and wallpapers from Wallhaven, Wikimedia Commons, Bing, APOD
  and Unsplash; it installs xz, zstd and lz4 packs and WebP, AVIF and
  JPEG XL pictures.
- Dr. Konqi's debugger can attach under Yama `ptrace_scope` 1.

### Removed

aRts, KJava, kcert, libkmid, kab, kspell2, kmdi, the khexedit, kscript and
kspeech interfaces, libkdefakes, kdecore/malloc, kdesasl, the JPEG 2000 and
OpenEXR kimgio plugins, kthemestyle, the non-CUPS print systems of kdeprint,
the Windows/Mac files, and the tests and examples of the 3.5.10 tarball.

## Requirements

- CMake 3.20 or later, a C and C++ compiler (GCC or Clang), pkg-config.
- [kde3-qt](https://github.com/kde3-revival/kde3-qt), installed (default
  `/opt/kde3-qt`). A stock Qt 3 is rejected. kde3-qt is found through
  `QT_DIR`/`QTDIR`, the `kde3-qt` pkg-config module, or `/opt/kde3-qt`.
- Required: the X11 libraries, zlib, libjpeg, libpng, bzip2,
  liblzma, libzstd, OpenSSL 3, libxml2, libxslt, CUPS, PCRE 1
  (`libpcre`, `libpcreposix`; can be turned off with
  `-DKDE3_WITH_PCRE=OFF`).
- Optional (used when found, each with a `KDE3_WITH_*` option): libacl and
  libattr, libutempter, libidn, libpsl, libsecret, librsvg 2.52 with cairo,
  libtiff, libwebp, libheif, libavif, libjxl, liblz4, libbrotlidec, GSSAPI
  (Kerberos), libdns_sd (Avahi's compatibility library), libfam,
  libpulse with libsndfile, fontconfig and FreeType.

At the end of configuring, CMake prints which features are enabled and
which packages were not found.

## Building and installing

kdelibs is built after kde3-qt and before kde3-base and every other KDE 3
module.

    cmake -S . -B build
    cmake --build build -j"$(nproc)"
    sudo cmake --install build

The default install prefix is `/opt/kde3`, so KDE 3 does not mix with the
distribution's KDE Plasma; change it with `-DCMAKE_INSTALL_PREFIX=...`.
Modules built later take the prefix from the installed kdelibs. The default
build type is `RelWithDebInfo`.

Useful options (all `-D<option>=ON|OFF`):

| Option | Default | Effect |
|---|---|---|
| `KDE3_WITH_<feature>` | ON | ACL, UTEMPTER, LIBIDN, LIBPSL, LIBSECRET, SVG, TIFF, WEBP, HEIF, AVIF, JXL, LZ4, BROTLI, PCRE, GSSAPI, DNSSD, FAM, PULSEAUDIO |
| `KDE3_WITH_LIBTHAI` | OFF | Thai line breaking with libthai |
| `KDE3_WITH_LUA` | OFF | Lua 5.0 indentation scripts in Kate |
| `KDE3_ENABLE_INOTIFY` / `KDE3_ENABLE_DNOTIFY` | ON / OFF | File change notification in KDirWatch |
| `KDE3_ENABLE_MITSHM` | ON | MIT-SHM for pixmap loading and saving |
| `KDE3_ENABLE_SENDFILE` | OFF | `sendfile()` in kio_file |
| `KDE3_KDESU_SUDO_BACKEND` | OFF | kdesu uses sudo instead of su |
| `KDE3_INSTALL_CUPS_PPD` | ON | Install `postscript.ppd.gz` into CUPS' model directory |
| `KDE3_ENABLE_WARNINGS` | ON | The warning flags of the KDE 3.5 autotools build |

`kgrantpty`, `start_kdeinit` and `kpac_dhcp_helper` are installed setuid
root; this only works when installing as root (packages set the mode
themselves).

## Packages

[kde3-packaging](https://github.com/kde3-revival/kde3-packaging) builds
this repository for Fedora (`kde3-kdelibs`, `kde3-kdelibs-devel`) and Arch
Linux (`kde3-kdelibs`). The packages install into `/opt/kde3` and add
`/opt/kde3/bin` at the end of `PATH`.

## Status

kdelibs is used every day as part of the kde3-revival desktop on X11.
Development and testing happen on current Fedora and Arch Linux; the BSD
code of KDE 3.5.10 is kept but not tested. The developer documentation in
`COMPILING`, `KDE3PORTING.html` and similar files is the original one and
describes the autotools build.

## License

The libraries are under the GNU Library General Public License (LGPL),
see `COPYING.LIB`; programs and other parts under the GNU General Public
License (GPL), see `COPYING`; some code under a BSD-style license, see
`COPYING.BSD`; the documentation under the GNU Free Documentation
License, see `COPYING-DOCS`. When in doubt, check the header of the
individual file. Contributions are accepted under the same terms.

## Related repositories

- [kde3-qt](https://github.com/kde3-revival/kde3-qt): Qt 3.3.8b, the base
  of everything
- [kde3-base](https://github.com/kde3-revival/kde3-base): the desktop
  (KWin, Kicker, KDesktop, Konqueror, Konsole, KControl, ...)
- [kde3-apps](https://github.com/kde3-revival/kde3-apps): applications
- [kde3-packaging](https://github.com/kde3-revival/kde3-packaging): Fedora
  and Arch Linux packages
