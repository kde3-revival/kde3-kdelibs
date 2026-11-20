/*
 * This file is part of the KDE libraries
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License version 2 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 */

#ifndef KSVGICONRENDER_P_H
#define KSVGICONRENDER_P_H

// Private to kdecore, only built with librsvg (HAVE_LIBRSVG).

class QImage;
class QString;

/*
 * Renders the SVG or gzip-compressed SVG (.svgz) file @p path into a
 * @p size x @p size image: the drawing is scaled to fit and centred,
 * keeping its aspect ratio.  With @p size <= 0 the size of the
 * document is used.  Returns a new 32-bit image with an alpha buffer
 * (not premultiplied, as Qt expects), or 0 if the file cannot be read
 * or rendered.  The caller owns the image.
 */
QImage *kdeRenderSvgIcon(const QString &path, int size);

#endif
