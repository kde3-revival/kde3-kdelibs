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

#include "kdlopen_p.h"

#include <string.h>

#include <qcstring.h>

int kde_dlopen_flags = RTLD_LAZY;

void *kde_dlopen(const char *filename)
{
    if (!filename)
        return 0;

    const size_t len = strlen(filename);
    if (len > 3 && strcmp(filename + len - 3, ".la") == 0) {
        QCString so(filename, len - 3 + 1); // copies without ".la"
        so += ".so";
        return dlopen(so.data(), kde_dlopen_flags);
    }
    return dlopen(filename, kde_dlopen_flags);
}
