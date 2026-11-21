/* This file is part of the KDE project
   Copyright (C) 2026 Serghei Amelian <serghei.amelian@gmail.com>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the Lesser GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
*/

#ifndef KIMG_AVIF_H
#define KIMG_AVIF_H

class QImageIO;

extern "C" {
void kimgio_avif_read( QImageIO * );
}

#endif
