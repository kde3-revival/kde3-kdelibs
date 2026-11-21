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

#ifndef KSHAREDMIMEMAGIC_H
#define KSHAREDMIMEMAGIC_H

#include <qcstring.h>
#include <qstring.h>
#include <qstringlist.h>
#include <qmap.h>
#include <qvaluevector.h>
#include <kdelibs_export.h>

/**
 * @internal
 * The magic rules of the freedesktop.org shared-mime-info database: the
 * binary "magic" file (format 1.0) update-mime-database writes to
 * $XDG_DATA_DIRS/mime.
 *
 * The files are untrusted input: every length and offset is checked, the
 * nesting depth is bounded, and so is the work done by one match().
 */
class KDE_NO_EXPORT KSharedMimeMagic
{
public:
    KSharedMimeMagic();

    /**
     * Reads the magic files in @p dirs, most important first (the rules
     * a directory has for a type replace those of the directories after it).
     */
    void load( const QStringList &dirs );

    /**
     * Parses one magic file's contents. The sections of the types in
     * @p skipTypes are left out; the types read are added to it.
     * Returns false if @p data is not a magic file.
     */
    bool parse( const QByteArray &data, QMap<QString,bool> *skipTypes = 0 );

    /** Sorts the sections, highest priority first. Called by load(). */
    void finish();

    /**
     * The type whose rules match @p data, of the highest priority, or
     * QString::null. @p priority receives its priority.
     */
    QString match( const unsigned char *data, int len, int *priority = 0 ) const;

    uint sectionCount() const { return m_sections.count(); }

private:
    struct Rule
    {
        Q_UINT32 indent;
        Q_UINT32 offset;
        Q_UINT32 range;
        QByteArray value;
        QByteArray mask;   // empty: no mask
        Q_UINT64 firstBytes[4]; // the bytes the value may start with
    };
    struct Section
    {
        int priority;
        QString type;
        uint first;        // index of its first rule in m_rules
        uint count;
        // if all its rules of indent 0 look at one offset: that offset,
        // and the bytes they may start with there; else -1
        int anchor;
        Q_UINT64 anchorBytes[4];
    };

    bool matchRule( uint index, uint end, const unsigned char *data, uint len,
                    Q_UINT32 &budget ) const;
    static bool canStartWith( const Rule &rule, uchar byte )
    { return ( rule.firstBytes[byte >> 6] >> ( byte & 63 ) ) & 1; }

    QValueVector<Rule> m_rules;
    QValueVector<Section> m_sections;
};

#endif
