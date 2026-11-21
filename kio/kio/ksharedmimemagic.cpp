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

#include "ksharedmimemagic.h"

#include <qfile.h>
#include <qmap.h>

#include <kdebug.h>

#include <algorithm>
#include <string.h>

/*
 * The format (shared-mime-info specification, "The magic files"):
 *
 *   "MIME-Magic\0\n"
 *   then sections:
 *   "[" priority ":" mime-type "]\n"
 *   then lines:
 *   [ indent ] ">" start-offset "=" value-length(2 bytes, big endian) value
 *              [ "&" mask ] [ "~" word-size ] [ "+" range-length ] "\n"
 *
 * A line matches if the value (masked) is found at start-offset, or at
 * one of the range-length offsets from there, and one of the lines
 * indented one more below it matches (if there are any). A section
 * matches if one of its lines of indent 0 matches. A line with unknown
 * contents where "\n" is expected is ignored.
 */

static const char s_header[] = "MIME-Magic\0\n";
static const uint s_headerLen = 12;

// Limits for hostile files
static const uint s_maxFileSize = 16 * 1024 * 1024;
static const uint s_maxIndent = 32;
static const uint s_maxRules = 200000;
static const uint s_maxSections = 65536;
static const Q_UINT32 s_maxNumber = 0x7fffffff;
// Byte comparisons one match() may make
static const Q_UINT32 s_matchBudget = 16 * 1024 * 1024;

KSharedMimeMagic::KSharedMimeMagic()
{
}

void KSharedMimeMagic::load( const QStringList &dirs )
{
    QMap<QString,bool> seenTypes;
    for ( QStringList::ConstIterator it = dirs.begin(); it != dirs.end(); ++it )
    {
        QString path = *it;
        if ( !path.endsWith( "/" ) )
            path += '/';
        QFile file( path + "magic" );
        if ( !file.exists() || !file.open( IO_ReadOnly ) )
            continue;
        if ( file.size() > s_maxFileSize )
        {
            kdWarning(7018) << "Ignoring oversized " << file.name() << endl;
            continue;
        }
        QByteArray data = file.readAll();
        // The rules a directory has for a type replace those of the
        // directories after it (a choice: xdgmime merges them)
        if ( !parse( data, &seenTypes ) )
            kdWarning(7018) << file.name() << " is not a MIME magic file" << endl;
    }
    finish();
}

// Reads a decimal number at data[pos]; false if there is none
static bool readNumber( const QByteArray &data, uint &pos, Q_UINT32 &value )
{
    const uint size = data.size();
    const uint start = pos;
    Q_UINT32 v = 0;
    while ( pos < size && data[pos] >= '0' && data[pos] <= '9' )
    {
        const Q_UINT32 digit = data[pos] - '0';
        if ( v > ( s_maxNumber - digit ) / 10 )
            v = s_maxNumber; // saturate: such an offset can't match anyway
        else
            v = v * 10 + digit;
        ++pos;
    }
    value = v;
    return pos > start;
}

static void skipLine( const QByteArray &data, uint &pos )
{
    const uint size = data.size();
    while ( pos < size && data[pos] != '\n' )
        ++pos;
    if ( pos < size )
        ++pos;
}

bool KSharedMimeMagic::parse( const QByteArray &data, QMap<QString,bool> *skipTypes )
{
    const uint size = data.size();
    if ( size < s_headerLen || memcmp( data.data(), s_header, s_headerLen ) != 0 )
        return false;

    // the types of this file: they are skipped in the next files only
    QMap<QString,bool> fileTypes;
    uint pos = s_headerLen;
    Section section;
    section.priority = 0;
    section.first = 0;
    section.count = 0;
    bool inSection = false;     // reading the rules of a section we keep
    bool broken = false;        // a line of the section could not be used
    // indent of the previous rule of the section, to find orphans
    int lastIndent = -1;

    // A section with a line that could not be used is dropped whole, like
    // xdgmime does: without a line, its parent could match on its own
#define END_SECTION() \
    do { \
        if ( inSection && !broken && section.count && m_sections.count() < s_maxSections ) \
            m_sections.push_back( section ); \
        else if ( inSection ) \
            m_rules.erase( m_rules.begin() + section.first, m_rules.end() ); \
        inSection = false; \
    } while ( 0 )

    while ( pos < size )
    {
        if ( data[pos] == '[' )
        {
            END_SECTION();
            ++pos;
            Q_UINT32 priority;
            if ( !readNumber( data, pos, priority ) || pos >= size || data[pos] != ':' )
            {
                skipLine( data, pos );
                continue;
            }
            ++pos;
            const uint start = pos;
            while ( pos < size && data[pos] != ']' && data[pos] != '\n' && pos - start < 256 )
                ++pos;
            if ( pos >= size || data[pos] != ']' || pos + 1 >= size || data[pos + 1] != '\n' )
            {
                skipLine( data, pos );
                continue;
            }
            const QString type = QString::fromLatin1( data.data() + start, pos - start );
            pos += 2;
            section.priority = QMIN( (int)priority, 100 );
            section.type = type;
            section.first = m_rules.count();
            section.count = 0;
            lastIndent = -1;
            broken = false;
            inSection = !( skipTypes && skipTypes->contains( type ) ) && type.find( '/' ) > 0 &&
                        m_sections.count() < s_maxSections;
            if ( fileTypes.count() < s_maxSections )
                fileTypes.insert( type, true );
            continue;
        }

        // A rule line
        Rule rule;
        rule.indent = 0;
        rule.range = 1;
        readNumber( data, pos, rule.indent );
        if ( pos >= size || data[pos] != '>' )
        {
            skipLine( data, pos );
            broken = true;
            continue;
        }
        ++pos;
        if ( !readNumber( data, pos, rule.offset ) || pos + 3 > size || data[pos] != '=' )
        {
            skipLine( data, pos );
            broken = true;
            continue;
        }
        ++pos;
        const uint valueLen = ( (uchar)data[pos] << 8 ) | (uchar)data[pos + 1];
        pos += 2;
        if ( valueLen > size - pos )
        {
            // can't know where the line ends: give up on the file
            kdWarning(7018) << "Truncated MIME magic rule" << endl;
            broken = true;
            break;
        }
        rule.value.duplicate( data.data() + pos, valueLen );
        pos += valueLen;
        if ( pos < size && data[pos] == '&' )
        {
            ++pos;
            if ( valueLen > size - pos )
            {
                kdWarning(7018) << "Truncated MIME magic mask" << endl;
                broken = true;
                break;
            }
            rule.mask.duplicate( data.data() + pos, valueLen );
            pos += valueLen;
        }
        Q_UINT32 wordSize = 1;
        if ( pos < size && data[pos] == '~' )
        {
            ++pos;
            readNumber( data, pos, wordSize );
        }
        if ( pos < size && data[pos] == '+' )
        {
            ++pos;
            readNumber( data, pos, rule.range );
            if ( rule.range == 0 )
                rule.range = 1;
        }
        if ( pos >= size || data[pos] != '\n' )
        {
            // unknown extension
            skipLine( data, pos );
            broken = true;
            continue;
        }
        ++pos;

        if ( !inSection || broken )
            continue;
        // "__NOMAGIC__": no rules, only the replacing of other directories'
        if ( rule.indent == 0 && valueLen == 11 && memcmp( rule.value.data(), "__NOMAGIC__", 11 ) == 0 )
            continue;
        // A rule nested deeper than one below the previous has no parent
        if ( valueLen == 0 || rule.indent > s_maxIndent || (int)rule.indent > lastIndent + 1 ||
             m_rules.count() >= s_maxRules )
        {
            broken = true;
            continue;
        }

        // Host-endian values (word size 2 or 4) are stored big endian
        if ( ( wordSize == 2 || wordSize == 4 ) && valueLen % wordSize == 0 )
        {
            const Q_UINT16 probe = 1;
            const bool littleEndian = *(const uchar *)&probe == 1;
            if ( littleEndian )
            {
                for ( uint i = 0; i < valueLen; i += wordSize )
                {
                    std::reverse( rule.value.data() + i, rule.value.data() + i + wordSize );
                    if ( !rule.mask.isEmpty() )
                        std::reverse( rule.mask.data() + i, rule.mask.data() + i + wordSize );
                }
            }
        }

        const uchar v0 = rule.value[0];
        const uchar m0 = rule.mask.isEmpty() ? 0xff : (uchar)rule.mask[0];
        for ( int b = 0; b < 4; ++b )
            rule.firstBytes[b] = 0;
        for ( int b = 0; b < 256; ++b )
            if ( ( b & m0 ) == ( v0 & m0 ) )
                rule.firstBytes[b >> 6] |= Q_UINT64( 1 ) << ( b & 63 );

        lastIndent = rule.indent;
        m_rules.push_back( rule );
        ++section.count;
    }
    END_SECTION();
#undef END_SECTION

    if ( skipTypes )
        for ( QMap<QString,bool>::ConstIterator it = fileTypes.begin(); it != fileTypes.end(); ++it )
            skipTypes->insert( it.key(), true );
    return true;
}

namespace {
struct ByPriority
{
    template<class S> bool operator()( const S &a, const S &b ) const
    { return a.priority > b.priority; }
};
}

void KSharedMimeMagic::finish()
{
    for ( QValueVector<Section>::Iterator s = m_sections.begin(); s != m_sections.end(); ++s )
    {
        Section &sec = *s;
        sec.anchor = -1;
        for ( int b = 0; b < 4; ++b )
            sec.anchorBytes[b] = 0;
        bool anchored = true;
        const uint end = sec.first + sec.count;
        for ( uint i = sec.first; i < end && anchored; ++i )
        {
            const Rule &rule = m_rules[i];
            if ( rule.indent != 0 )
                continue;
            if ( rule.range != 1 || rule.offset > 0x7fffffff ||
                 ( sec.anchor != -1 && sec.anchor != (int)rule.offset ) )
                anchored = false;
            else
            {
                sec.anchor = rule.offset;
                for ( int b = 0; b < 4; ++b )
                    sec.anchorBytes[b] |= rule.firstBytes[b];
            }
        }
        if ( !anchored )
            sec.anchor = -1;
    }
    std::stable_sort( m_sections.begin(), m_sections.end(), ByPriority() );
}

bool KSharedMimeMagic::matchRule( uint index, uint end, const unsigned char *data, uint len,
                                  Q_UINT32 &budget ) const
{
    const Rule &rule = m_rules[index];
    const uint valueLen = rule.value.size();
    if ( valueLen > len || rule.offset > len - valueLen )
        return false;
    // last offset to try, within the data
    Q_UINT32 last = len - valueLen;
    if ( rule.range - 1 < last - rule.offset )
        last = rule.offset + rule.range - 1;

    const uchar *value = (const uchar *)rule.value.data();
    const uchar *mask = rule.mask.isEmpty() ? 0 : (const uchar *)rule.mask.data();
    const uchar *p = data + rule.offset;
    const uchar *lastp = data + last;
    // every offset scanned costs one
    const Q_UINT32 scan = last - rule.offset + 1;
    if ( budget < scan )
        return false;
    budget -= scan;
    bool found = false;
    if ( !mask )
    {
        // candidates by their first byte, then the rest
        while ( p <= lastp )
        {
            p = (const uchar *)memchr( p, value[0], lastp - p + 1 );
            if ( !p )
                break;
            if ( budget < valueLen )
                return false;
            budget -= valueLen;
            if ( memcmp( p + 1, value + 1, valueLen - 1 ) == 0 )
            {
                found = true;
                break;
            }
            ++p;
        }
    }
    else
    {
        const uchar first = value[0] & mask[0];
        for ( ; p <= lastp && !found; ++p )
        {
            if ( ( p[0] & mask[0] ) != first )
                continue;
            if ( budget < valueLen )
                return false;
            budget -= valueLen;
            uint i = 1;
            while ( i < valueLen && ( p[i] & mask[i] ) == ( value[i] & mask[i] ) )
                ++i;
            found = i == valueLen;
        }
    }
    if ( !found )
        return false;

    // The lines below it: one of them must match too
    bool hasChildren = false;
    for ( uint i = index + 1; i < end && m_rules[i].indent > rule.indent; ++i )
    {
        if ( !budget )
            return false;
        --budget;
        if ( m_rules[i].indent != rule.indent + 1 )
            continue;
        hasChildren = true;
        if ( matchRule( i, end, data, len, budget ) )
            return true;
    }
    return !hasChildren;
}

QString KSharedMimeMagic::match( const unsigned char *data, int len, int *priority ) const
{
    if ( !data || len <= 0 )
        return QString::null;
    Q_UINT32 budget = s_matchBudget;
    for ( QValueVector<Section>::ConstIterator s = m_sections.begin(); s != m_sections.end(); ++s )
    {
        const int anchor = (*s).anchor;
        if ( anchor >= 0 )
        {
            if ( anchor >= len )
                continue;
            const uchar b = data[anchor];
            if ( !( ( (*s).anchorBytes[b >> 6] >> ( b & 63 ) ) & 1 ) )
                continue;
        }
        const uint end = (*s).first + (*s).count;
        for ( uint i = (*s).first; i < end; ++i )
        {
            const Rule &rule = m_rules[i];
            if ( rule.indent != 0 )
                continue;
            // most rules are a value at one offset: look at its first byte
            if ( rule.range == 1 &&
                 ( rule.offset >= (uint)len || !canStartWith( rule, data[rule.offset] ) ) )
                continue;
            if ( matchRule( i, end, data, len, budget ) )
            {
                if ( priority )
                    *priority = (*s).priority;
                return (*s).type;
            }
            if ( !budget )
                return QString::null;
        }
    }
    return QString::null;
}
