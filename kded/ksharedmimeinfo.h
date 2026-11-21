/*  This file is part of the KDE libraries
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License version 2 as published by the Free Software Foundation;
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 **/

#ifndef KSHAREDMIMEINFO_H
#define KSHAREDMIMEINFO_H

#include <qstring.h>
#include <qstringlist.h>
#include <qdict.h>

/**
 * @internal
 * One MIME type of the freedesktop.org shared-mime-info database, as
 * update-mime-database writes it to $XDG_DATA_DIRS/mime/<media>/<sub>.xml.
 *
 * These per-type files hold everything kbuildsycoca needs (the globs with
 * their weights and case sensitivity, aliases, parents, icons and the
 * localized comments), and there is one file per type, which is what the
 * sycoca builder expects of a resource: one entry per file, re-read only
 * when that file changes. The magic rules are not in them; KMimeMagic reads
 * the generated binary "magic" file at run time instead.
 */
struct KSharedMimeInfoType
{
    QString name;
    QString comment;
    QString icon;           // <icon name=...>, empty if none
    QString genericIcon;    // <generic-icon name=...>, empty if none
    QStringList aliases;
    QStringList subClassOf;
    QStringList globs;      // "weight:flags:pattern", flags "cs" or ""
    QStringList patterns;   // the patterns alone, in the same order
    bool globDeleteAll;     // <glob-deleteall/>: ignore the less important globs

    KSharedMimeInfoType() : globDeleteAll( false ) {}
};

namespace KSharedMimeInfo
{
    /**
     * Parses one per-type XML file. @p languages is the list of languages
     * to choose the comment from, most preferred first.
     * Returns false if the file is not a usable type definition.
     */
    bool parseTypeFile( const QString &path, const QStringList &languages,
                        KSharedMimeInfoType &type );

    /**
     * Adds to @p type what @p lessImportant (the same type, from a directory
     * after @p type's in $XDG_DATA_DIRS) has and @p type has not.
     */
    void merge( KSharedMimeInfoType &type, const KSharedMimeInfoType &lessImportant );

    /** Whether @p name looks like a MIME type name ("media/subtype"). */
    bool isValidTypeName( const QString &name );

    /**
     * Chooses the KDE icon name for @p type: the type's own icon, the
     * freedesktop.org default name and the generic icon, whichever the icon
     * loader finds first; otherwise the closest KDE 3 icon.
     */
    QString chooseIcon( const KSharedMimeInfoType &type );
}

#endif
