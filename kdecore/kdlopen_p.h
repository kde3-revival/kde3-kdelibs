/* This file is part of the KDE libraries

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License version 2 as published by the Free Software Foundation.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#ifndef KDLOPEN_P_H
#define KDLOPEN_P_H

/*
 * Private replacement for the bits of the bundled libltdl that KDE used
 * (not installed).  Symbols are looked up with dlsym(), errors are read
 * with dlerror() and handles are released with dlclose() directly.
 */

#include <dlfcn.h>

#include <kdelibs_export.h>

/**
 * The flags kde_dlopen() passes to dlopen(): RTLD_LAZY by default.
 * KLibLoader::globalLibrary() and kdeinit temporarily OR RTLD_GLOBAL
 * into it (this replaces libltdl's lt_dlopen_flag).
 */
extern KDECORE_EXPORT int kde_dlopen_flags;

/**
 * dlopen() with kde_dlopen_flags.  A trailing ".la" (libtool library
 * names used by KDE 3 configuration files and callers) is replaced by
 * ".so"; no libtool archives are installed.
 */
KDECORE_EXPORT void *kde_dlopen(const char *filename);

#endif
