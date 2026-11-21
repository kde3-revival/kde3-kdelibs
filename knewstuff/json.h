/*
    This file is part of KNewStuff.
    Copyright (c) 2026 The KDE 3 revival authors

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/
#ifndef KNEWSTUFF_JSON_H
#define KNEWSTUFF_JSON_H

// Private to libknewstuff, not installed.

#include <qcstring.h>
#include <qstring.h>
#include <qvariant.h>

namespace KNS {

/**
 * A small JSON reader (RFC 8259) for the untrusted answers of the
 * wallpaper sources. Objects become QVariant maps, arrays QVariant lists,
 * numbers doubles, true/false bools and null an invalid QVariant.
 *
 * It refuses: nesting deeper than 32 levels, more than 200000 values,
 * strings with unescaped control characters, invalid UTF-8 (replaced
 * instead), lone surrogates (replaced), trailing data. Of duplicated keys
 * the first one counts.
 */
bool parseJson( const QByteArray &data, QVariant *result, QString *error = 0 );

/** Convenience accessors; a missing or mistyped value gives the default. */
QVariant jsonValue( const QVariant &object, const char *key );
QString jsonString( const QVariant &object, const char *key );
double jsonNumber( const QVariant &object, const char *key, double def = 0 );
QValueList<QVariant> jsonList( const QVariant &object, const char *key );

/**
 * Plain text of an HTML fragment from a server (author, credit and
 * description fields): tags dropped, entities decoded, control characters
 * and runs of white space collapsed, at most @p max characters.
 */
QString htmlToPlainText( const QString &html, uint max );

}

#endif
