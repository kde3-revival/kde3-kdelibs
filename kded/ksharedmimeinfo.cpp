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

#include "ksharedmimeinfo.h"

#include <qfile.h>
#include <qmap.h>
#include <qregexp.h>
#include <qxml.h>

#include <kglobal.h>
#include <kiconloader.h>
#include <kdebug.h>

// Per-type files are a few kB (most of it translated comments); anything
// much larger is not a file update-mime-database wrote.
static const uint s_maxTypeFileSize = 1024 * 1024;

bool KSharedMimeInfo::isValidTypeName( const QString &name )
{
    if ( name.isEmpty() || name.length() > 255 )
        return false;
    int slash = -1;
    for ( uint i = 0; i < name.length(); ++i )
    {
        const ushort c = name[i].unicode();
        if ( c == '/' )
        {
            if ( slash != -1 )
                return false;
            slash = i;
            continue;
        }
        if ( c <= ' ' || c >= 127 || c == ';' || c == ',' || c == '"' || c == '\\' )
            return false;
    }
    return slash > 0 && slash < (int)name.length() - 1;
}

static bool isValidPattern( const QString &pattern )
{
    if ( pattern.isEmpty() || pattern.length() > 255 )
        return false;
    // A glob matches a file name: no directory separators, and ';' is the
    // list separator of the KDE Patterns= key.
    return pattern.find( '/' ) == -1 && pattern.find( ';' ) == -1;
}

// "pt-BR", "sr@latin", "zh_CN" -> "pt_br", "sr@latin", "zh_cn"
static QString normalizedLanguage( const QString &lang )
{
    QString l = lang.lower();
    l.replace( '-', '_' );
    return l;
}

namespace {

class TypeFileHandler : public QXmlDefaultHandler
{
public:
    TypeFileHandler( KSharedMimeInfoType &type )
        : m_type( type ), m_depth( 0 ), m_inComment( false ), m_root( false ) {}

    bool startElement( const QString &, const QString &localName,
                       const QString &qName, const QXmlAttributes &atts )
    {
        const QString tag = localName.isEmpty() ? qName : localName;
        ++m_depth;
        if ( m_depth == 1 )
        {
            if ( tag != "mime-type" )
                return false;
            m_root = true;
            m_type.name = atts.value( "type" );
            return true;
        }
        if ( m_depth != 2 )
            return true; // inside <magic>, <root-XML>, ... : not used here

        if ( tag == "comment" )
        {
            m_inComment = true;
            m_text = QString::null;
            m_lang = atts.value( "xml:lang" );
            if ( m_lang.isEmpty() )
                m_lang = atts.value( "http://www.w3.org/XML/1998/namespace", "lang" );
        }
        else if ( tag == "glob" )
        {
            const QString pattern = atts.value( "pattern" );
            if ( !isValidPattern( pattern ) )
                return true;
            bool ok = false;
            int weight = atts.value( "weight" ).toInt( &ok );
            if ( !ok )
                weight = 50;
            weight = QMAX( 0, QMIN( weight, 100 ) );
            const bool cs = atts.value( "case-sensitive" ) == "true";
            // A glob that is not case-sensitive matches the lowercased
            // file name, so it is kept in lowercase (update-mime-database
            // already writes it so).
            const QString p = cs ? pattern : pattern.lower();
            if ( m_type.patterns.contains( p ) )
                return true;
            m_type.patterns.append( p );
            m_type.globs.append( QString::number( weight ) + ( cs ? ":cs:" : "::" ) + p );
        }
        else if ( tag == "alias" )
        {
            const QString a = atts.value( "type" );
            if ( KSharedMimeInfo::isValidTypeName( a ) && !m_type.aliases.contains( a ) )
                m_type.aliases.append( a );
        }
        else if ( tag == "sub-class-of" )
        {
            const QString p = atts.value( "type" );
            if ( KSharedMimeInfo::isValidTypeName( p ) && !m_type.subClassOf.contains( p ) )
                m_type.subClassOf.append( p );
        }
        else if ( tag == "glob-deleteall" )
            m_type.globDeleteAll = true;
        else if ( tag == "generic-icon" )
            m_type.genericIcon = atts.value( "name" );
        else if ( tag == "icon" )
            m_type.icon = atts.value( "name" );
        return true;
    }

    bool endElement( const QString &, const QString &, const QString & )
    {
        if ( m_depth == 2 && m_inComment )
        {
            m_inComment = false;
            const QString text = m_text.simplifyWhiteSpace();
            if ( !text.isEmpty() && m_comments.count() < 1000 )
            {
                const QString key = m_lang.isEmpty() ? QString::fromLatin1( "" )
                                                     : normalizedLanguage( m_lang );
                if ( !m_comments.contains( key ) )
                    m_comments.insert( key, text );
            }
        }
        --m_depth;
        return true;
    }

    bool characters( const QString &ch )
    {
        if ( m_inComment && m_text.length() < 4096 )
            m_text += ch;
        return true;
    }

    QString comment( const QStringList &languages ) const
    {
        for ( QStringList::ConstIterator it = languages.begin(); it != languages.end(); ++it )
        {
            const QString lang = normalizedLanguage( *it );
            if ( lang.isEmpty() || lang == "c" || lang == "en_us" )
                break;
            QMap<QString,QString>::ConstIterator c = m_comments.find( lang );
            if ( c != m_comments.end() )
                return c.data();
            // "pt_br" -> "pt", "sr@latn" -> "sr"
            int cut = lang.find( QRegExp( "[_@.]" ) );
            if ( cut > 0 )
            {
                c = m_comments.find( lang.left( cut ) );
                if ( c != m_comments.end() )
                    return c.data();
            }
        }
        QMap<QString,QString>::ConstIterator c = m_comments.find( QString::fromLatin1( "" ) );
        return c != m_comments.end() ? c.data() : QString::null;
    }

    bool sawRoot() const { return m_root; }

private:
    KSharedMimeInfoType &m_type;
    int m_depth;
    bool m_inComment;
    bool m_root;
    QString m_text;
    QString m_lang;
    QMap<QString,QString> m_comments;
};

}

bool KSharedMimeInfo::parseTypeFile( const QString &path, const QStringList &languages,
                                     KSharedMimeInfoType &type )
{
    QFile file( path );
    if ( !file.open( IO_ReadOnly ) )
        return false;
    if ( file.size() > s_maxTypeFileSize )
    {
        kdWarning(7021) << "Ignoring oversized MIME type file " << path << endl;
        return false;
    }

    type = KSharedMimeInfoType();
    TypeFileHandler handler( type );
    QXmlInputSource source( &file );
    QXmlSimpleReader reader;
    reader.setContentHandler( &handler );
    if ( !reader.parse( &source, false ) || !handler.sawRoot() )
    {
        kdWarning(7021) << "Could not parse MIME type file " << path << endl;
        return false;
    }
    if ( !isValidTypeName( type.name ) )
        return false;
    type.comment = handler.comment( languages );
    type.aliases.remove( type.name );
    type.subClassOf.remove( type.name );
    return true;
}

void KSharedMimeInfo::merge( KSharedMimeInfoType &type, const KSharedMimeInfoType &other )
{
    if ( type.comment.isEmpty() )
        type.comment = other.comment;
    if ( type.icon.isEmpty() )
        type.icon = other.icon;
    if ( type.genericIcon.isEmpty() )
        type.genericIcon = other.genericIcon;
    for ( QStringList::ConstIterator it = other.aliases.begin(); it != other.aliases.end(); ++it )
        if ( !type.aliases.contains( *it ) )
            type.aliases.append( *it );
    for ( QStringList::ConstIterator it = other.subClassOf.begin(); it != other.subClassOf.end(); ++it )
        if ( !type.subClassOf.contains( *it ) )
            type.subClassOf.append( *it );
    if ( type.globDeleteAll )
        return;
    QStringList::ConstIterator g = other.globs.begin();
    for ( QStringList::ConstIterator p = other.patterns.begin(); p != other.patterns.end(); ++p, ++g )
        if ( !type.patterns.contains( *p ) )
        {
            type.patterns.append( *p );
            type.globs.append( *g );
        }
    type.globDeleteAll = other.globDeleteAll;
}

// Generic freedesktop.org icon names and what KDE 3 (crystalsvg) calls them
static const char * const s_genericIcons[][2] = {
    { "text-x-generic", "txt" },
    { "text-x-generic-template", "txt" },
    { "text-x-preview", "txt" },
    { "text-x-script", "shellscript" },
    { "text-html", "html" },
    { "text-x-csrc", "source_c" },
    { "text-x-chdr", "source_h" },
    { "text-x-c++src", "source_cpp" },
    { "text-x-python", "source_py" },
    { "text-x-java", "source_java" },
    { "application-x-executable", "exec" },
    { "application-x-sharedlib", "binary" },
    { "application-x-addon", "binary" },
    { "application-x-firmware", "binary" },
    { "application-x-generic", "unknown" },
    { "application-pdf", "pdf" },
    { "application-postscript", "postscript" },
    { "package-x-generic", "tar" },
    { "image-x-generic", "image" },
    { "audio-x-generic", "sound" },
    { "video-x-generic", "video" },
    { "font-x-generic", "font" },
    { "x-office-document", "wordprocessing" },
    { "x-office-document-template", "wordprocessing" },
    { "x-office-spreadsheet", "spreadsheet" },
    { "x-office-spreadsheet-template", "spreadsheet" },
    { "x-office-presentation", "document" },
    { "x-office-presentation-template", "document" },
    { "x-office-drawing", "vectorgfx" },
    { "x-office-drawing-template", "vectorgfx" },
    { "x-office-calendar", "vcalendar" },
    { "x-office-address-book", "vcard" },
    { "media-optical", "cdimage" },
    { "inode-directory", "folder" },
    { 0, 0 }
};

static QString kde3Icon( const QString &generic )
{
    for ( int i = 0; s_genericIcons[i][0]; ++i )
        if ( generic == s_genericIcons[i][0] )
            return QString::fromLatin1( s_genericIcons[i][1] );
    return QString::null;
}

static bool iconExists( const QString &name )
{
    if ( name.isEmpty() )
        return false;
    // The names of all the icons of the theme and the themes it inherits
    // (hicolor, in /usr/share/icons too), of any size: listing them once
    // is much cheaper than looking up each name.
    static QDict<void> *icons = 0;
    if ( !icons )
    {
        icons = new QDict<void>( 16411 );
        const QStringList paths = KGlobal::iconLoader()->queryIconsByContext( -KIcon::SizeMedium, KIcon::Any );
        for ( QStringList::ConstIterator it = paths.begin(); it != paths.end(); ++it )
        {
            QString icon = (*it).mid( (*it).findRev( '/' ) + 1 );
            const int dot = icon.findRev( '.' );
            if ( dot > 0 )
                icon.truncate( dot );
            if ( !icon.isEmpty() && !icons->find( icon ) )
                icons->insert( icon, (void *)1 );
        }
    }
    return icons->find( name ) != 0;
}

QString KSharedMimeInfo::chooseIcon( const KSharedMimeInfoType &type )
{
    const int slash = type.name.find( '/' );
    const QString media = type.name.left( slash );
    QString defaultIcon = type.name;
    defaultIcon.replace( '/', '-' );
    const QString generic = type.genericIcon.isEmpty()
        ? media + QString::fromLatin1( "-x-generic" ) : type.genericIcon;

    QStringList candidates;
    if ( !type.icon.isEmpty() )
        candidates.append( type.icon );
    candidates.append( defaultIcon );
    candidates.append( generic );
    for ( QStringList::ConstIterator it = candidates.begin(); it != candidates.end(); ++it )
        if ( iconExists( *it ) )
            return *it;

    QString icon = kde3Icon( generic );
    if ( icon.isEmpty() )
    {
        if ( media == "text" )
            icon = "txt";
        else if ( media == "image" )
            icon = "image";
        else if ( media == "audio" )
            icon = "sound";
        else if ( media == "video" )
            icon = "video";
        else if ( media == "font" )
            icon = "font";
        else if ( media == "message" || media == "multipart" )
            icon = "message";
        else if ( media == "model" )
            icon = "vectorgfx";
        else
            icon = "unknown";
    }
    return icon;
}
