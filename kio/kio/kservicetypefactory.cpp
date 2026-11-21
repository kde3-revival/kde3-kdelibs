/*  This file is part of the KDE libraries
 *  Copyright (C) 1999 Waldo Bastian <bastian@kde.org>
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

#include "kservicetypefactory.h"
#include "ksycoca.h"
#include "ksycocatype.h"
#include "ksycocadict.h"
#include "kservicetype.h"
#include "kmimetype.h"
#include "kuserprofile.h"

#include <kapplication.h>
#include <kdebug.h>
#include <assert.h>
#include <kstringhandler.h>
#include <qfile.h>
#include <qmap.h>


class KServiceTypeFactoryPrivate
{
public:
   KServiceTypeFactoryPrivate()
     : aliasOffset( 0 ), aliasesLoaded( false ), patternsLoaded( false ) {}

   Q_INT32 aliasOffset;
   bool aliasesLoaded;
   QMap<QString,QString> aliases;    // alias -> type name
   // patterns that also match the lowercased name (shared-mime-info ones
   // that are not case-sensitive)
   QMap<QString,bool> noCasePatterns;

   bool patternsLoaded;
   // "*.ext" patterns of the "other" list, by their suffix ".ext"
   QMap<QString,Q_INT32> suffixPatterns;
   // the rest of the "other" list (m_patterns): whether a pattern is a
   // glob that needs globMatch() ("*.[1-9]", "README*.txt")
   QValueVector<bool> globPatterns;
   // their first and last characters, if these are not wildcards: most
   // patterns are rejected by these without matching them
   QValueVector<QChar> firstChars;
   QValueVector<QChar> lastChars;
};

static bool isWildcard( QChar c )
{
   return c == '*' || c == '?' || c == '[' || c == ']';
}

// fnmatch(3) without flags, on QChars: '*', '?' and bracket expressions
// with ranges and '!' or '^'
static bool globMatch( const QChar *p, const QChar *pend, const QChar *n, const QChar *nend )
{
   const QChar *star = 0, *starName = 0;
   while ( n < nend )
   {
      if ( p < pend && *p == '*' )
      {
         star = ++p;
         starName = n;
         continue;
      }
      if ( p < pend && *p == '[' )
      {
         const QChar *q = p + 1;
         bool negate = false;
         if ( q < pend && ( *q == '!' || *q == '^' ) )
         {
            negate = true;
            ++q;
         }
         bool in = false, closed = false;
         bool firstChar = true;
         for ( ; q < pend; ++q, firstChar = false )
         {
            if ( *q == ']' && !firstChar )
            {
               closed = true;
               break;
            }
            if ( q + 2 < pend && q[1] == '-' && q[2] != ']' )
            {
               if ( *n >= q[0] && *n <= q[2] )
                  in = true;
               q += 2;
            }
            else if ( *q == *n )
               in = true;
         }
         if ( closed && in != negate )
         {
            p = q + 1;
            ++n;
            continue;
         }
         if ( !closed && *p == *n ) // a lone '[' is itself
         {
            ++p;
            ++n;
            continue;
         }
      }
      else if ( p < pend && ( *p == '?' || *p == *n ) )
      {
         ++p;
         ++n;
         continue;
      }
      if ( !star )
         return false;
      p = star;
      n = ++starName;
   }
   while ( p < pend && *p == '*' )
      ++p;
   return p == pend;
}

static bool globMatch( const QString &pattern, const QString &name )
{
   return globMatch( pattern.unicode(), pattern.unicode() + pattern.length(),
                     name.unicode(), name.unicode() + name.length() );
}

// A pattern of the form "*.suffix" with no other wildcard
static bool isSuffixPattern( const QString &pattern )
{
   if ( pattern.length() < 3 || pattern[0] != '*' || pattern[1] != '.' )
      return false;
   for ( uint i = 1; i < pattern.length(); ++i )
   {
      const QChar c = pattern[i];
      if ( c == '*' || c == '?' || c == '[' )
         return false;
   }
   return true;
}

// Whether KStringHandler::matchFileName() can't match @p pattern, which
// knows only a leading and/or a trailing '*'. The KDE patterns never need
// more; the shared-mime-info ones may.
static bool isGlobPattern( const QString &pattern )
{
   const int len = pattern.length();
   for ( int i = 0; i < len; ++i )
   {
      const QChar c = pattern[i];
      if ( c == '?' || c == '[' || ( c == '*' && i != 0 && i != len - 1 ) )
         return true;
   }
   return false;
}

KServiceTypeFactory::KServiceTypeFactory()
 : KSycocaFactory( KST_KServiceTypeFactory )
{
   _self = this;
   d = new KServiceTypeFactoryPrivate;
   m_fastPatternOffset = 0;
   m_otherPatternOffset = 0;
   if (m_str)
   {
      // Read Header
      Q_INT32 i,n;
      (*m_str) >> i;
      m_fastPatternOffset = i;
      (*m_str) >> i;
      m_otherPatternOffset = i;
      (*m_str) >> n;

      if (n > 1024)
      {
         KSycoca::flagError();
      }
      else
      {
         QString str;
         for(;n;n--)
         {
            KSycocaEntry::read(*m_str, str);
            (*m_str) >> i;
            m_propertyTypeDict.insert(str, i);
         }
         (*m_str) >> i;
         d->aliasOffset = i;
      }
   }
}


KServiceTypeFactory::~KServiceTypeFactory()
{
  _self = 0L;
  KServiceTypeProfile::clear();
  delete d;
}

KServiceTypeFactory * KServiceTypeFactory::self()
{
  if (!_self)
    _self = new KServiceTypeFactory();
  return _self;
}

KServiceType * KServiceTypeFactory::findServiceTypeByName(const QString &_name)
{
   if (!m_sycocaDict) return 0L; // Error!
   assert (!KSycoca::self()->isBuilding());
   int offset = m_sycocaDict->find_string( _name );
   KServiceType * newServiceType = offset ? createEntry(offset) : 0;

   // Check whether the dictionary was right.
   if (newServiceType && (newServiceType->name() != _name))
   {
     // No it wasn't...
     delete newServiceType;
     newServiceType = 0; // Not found
   }
   if (!newServiceType)
   {
     // Maybe a mimetype alias (application/x-zip-compressed)
     const QString target = resolveAlias( _name );
     if ( !target.isEmpty() )
     {
       offset = m_sycocaDict->find_string( target );
       newServiceType = offset ? createEntry(offset) : 0;
       if (newServiceType && (newServiceType->name() != target))
       {
         delete newServiceType;
         newServiceType = 0;
       }
     }
   }
   return newServiceType;
}

QString KServiceTypeFactory::resolveAlias( const QString &_name )
{
   if ( !m_str || !d->aliasOffset )
      return QString::null;
   if ( !d->aliasesLoaded )
   {
      d->aliasesLoaded = true;
      QDataStream *str = m_str;
      str->device()->at( d->aliasOffset );
      Q_INT32 n;
      (*str) >> n;
      if ( n < 0 || n > 100000 )
      {
         KSycoca::flagError();
         return QString::null;
      }
      QString alias, target;
      for ( ; n > 0; --n )
      {
         KSycocaEntry::read( *str, alias );
         KSycocaEntry::read( *str, target );
         d->aliases.insert( alias, target );
      }
      (*str) >> n;
      if ( n < 0 || n > 100000 )
      {
         KSycoca::flagError();
         return QString::null;
      }
      QString pattern;
      for ( ; n > 0; --n )
      {
         KSycocaEntry::read( *str, pattern );
         d->noCasePatterns.insert( pattern, true );
      }
   }
   QMap<QString,QString>::ConstIterator it = d->aliases.find( _name );
   return it != d->aliases.end() ? it.data() : QString::null;
}

QVariant::Type KServiceTypeFactory::findPropertyTypeByName(const QString &_name)
{
   if (!m_sycocaDict)
      return QVariant::Invalid; // Error!

   assert (!KSycoca::self()->isBuilding());

   QMapConstIterator<QString,int> it = m_propertyTypeDict.find(_name);
   if (it != m_propertyTypeDict.end()) {
     return (QVariant::Type)it.data();
   }

   return QVariant::Invalid;
}

KMimeType * KServiceTypeFactory::findFromPattern(const QString &_filename, QString *match)
{
   // Assume we're NOT building a database
   if (!m_str) return 0;

   Q_INT32 matchingOffset = findPatternOffset( _filename, match );
   if ( !matchingOffset )
   {
      // The shared-mime-info rule: a pattern that is not case-sensitive
      // matches the lowercased name (foo.FCStd, PHOTO.WEBP)
      const QString lower = _filename.lower();
      if ( lower != _filename )
      {
         (void) resolveAlias( QString::null ); // loads the table
         QString pattern;
         matchingOffset = findPatternOffset( lower, &pattern );
         if ( matchingOffset && !d->noCasePatterns.contains( pattern ) )
            matchingOffset = 0;
         else if ( matchingOffset && match )
            *match = pattern;
      }
   }

   if ( matchingOffset ) {
      KServiceType *newServiceType = createEntry( matchingOffset );
      assert (newServiceType && newServiceType->isType( KST_KMimeType ));
      return (KMimeType *) newServiceType;
   }
   else
      return 0;
}

Q_INT32 KServiceTypeFactory::findPatternOffset(const QString &_filename, QString *match)
{
   // Get stream to the header
   QDataStream *str = m_str;

   str->device()->at( m_fastPatternOffset );

   Q_INT32 nrOfEntries;
   (*str) >> nrOfEntries;
   Q_INT32 entrySize;
   (*str) >> entrySize;

   Q_INT32 fastOffset =  str->device()->at( );

   Q_INT32 matchingOffset = 0;

   // Let's go for a binary search in the "fast" pattern index
   Q_INT32 left = 0;
   Q_INT32 right = nrOfEntries - 1;
   Q_INT32 middle;
   // Extract extension
   int lastDot = _filename.findRev('.');
   int ext_len = _filename.length() - lastDot - 1;
   if (lastDot != -1 && ext_len <= 4) // if no '.', skip the extension lookup
   {
      QString extension = _filename.right( ext_len );
      extension = extension.leftJustify(4);

      QString pattern;
      while (left <= right) {
         middle = (left + right) / 2;
         // read pattern at position "middle"
         str->device()->at( middle * entrySize + fastOffset );
         KSycocaEntry::read(*str, pattern);
         int cmp = pattern.compare( extension );
         if (cmp < 0)
            left = middle + 1;
         else if (cmp == 0) // found
         {
            (*str) >> matchingOffset;
            // don't return newServiceType - there may be an "other" pattern that
            // matches best this file, like *.tar.bz
            if (match)
                *match = "*."+pattern.stripWhiteSpace();
            break; // but get out of the fast patterns
         }
         else
            right = middle - 1;
      }
   }

   // Now try the "other" Pattern table
   if ( !d->patternsLoaded ) {
      d->patternsLoaded = true;
      str->device()->at( m_otherPatternOffset );

      QString pattern;
      Q_INT32 mimetypeOffset;

      while (true)
      {
         KSycocaEntry::read(*str, pattern);
         if (pattern.isEmpty()) // end of list
            break;
         (*str) >> mimetypeOffset;
         if ( isSuffixPattern( pattern ) )
         {
            d->suffixPatterns.insert( pattern.mid( 1 ), mimetypeOffset );
            continue;
         }
         m_patterns.push_back( pattern );
         m_pattern_offsets.push_back( mimetypeOffset );
         d->globPatterns.push_back( isGlobPattern( pattern ) );
         const QChar first = pattern[0];
         const QChar last = pattern[pattern.length() - 1];
         d->firstChars.push_back( isWildcard( first ) ? QChar::null : first );
         d->lastChars.push_back( isWildcard( last ) ? QChar::null : last );
      }
   }

   assert( m_patterns.size() == m_pattern_offsets.size() );

   // "*.suffix" patterns: the longest one that matches (*.tar.gz before
   // *.gz) wins over the fast patterns
   if ( !d->suffixPatterns.isEmpty() )
   {
      for ( int dot = _filename.find( '.' ); dot != -1; dot = _filename.find( '.', dot + 1 ) )
      {
         // "*.ext" up to 4 characters are fast patterns
         if ( dot == lastDot && ext_len <= 4 )
            break;
         QMap<QString,Q_INT32>::ConstIterator sp = d->suffixPatterns.find( _filename.mid( dot ) );
         if ( sp != d->suffixPatterns.end() )
         {
            matchingOffset = sp.data();
            if (match)
               *match = "*" + sp.key();
            break;
         }
      }
   }

   QStringList::const_iterator it = m_patterns.begin();
   QStringList::const_iterator end = m_patterns.end();
   QValueVector<Q_INT32>::const_iterator it_offset = m_pattern_offsets.begin();
   QValueVector<bool>::const_iterator it_glob = d->globPatterns.begin();
   QValueVector<QChar>::const_iterator it_first = d->firstChars.begin();
   QValueVector<QChar>::const_iterator it_last = d->lastChars.begin();
   const QChar nameFirst = _filename.isEmpty() ? QChar::null : _filename[0];
   const QChar nameLast = _filename.isEmpty() ? QChar::null : _filename[_filename.length() - 1];

   for ( ; it != end; ++it, ++it_offset, ++it_glob, ++it_first, ++it_last )
   {
      if ( ( !(*it_first).isNull() && *it_first != nameFirst ) ||
           ( !(*it_last).isNull() && *it_last != nameLast ) )
         continue;
      bool matches;
      if ( *it_glob )
         matches = globMatch( *it, _filename );
      else
         matches = KStringHandler::matchFileName( _filename, *it );
      if ( matches )
      {
         if ( !matchingOffset || !(*it).endsWith( "*" ) ) // *.html wins over Makefile.*
         {
             matchingOffset = *it_offset;
             if (match)
                *match = *it;
             break;
         }
      }
   }

   return matchingOffset;
}

KMimeType::List KServiceTypeFactory::allMimeTypes()
{
   KMimeType::List result;
   KSycocaEntry::List list = allEntries();
   for( KSycocaEntry::List::Iterator it = list.begin();
        it != list.end();
        ++it)
   {
      KMimeType *newMimeType = dynamic_cast<KMimeType *>((*it).data());
      if (newMimeType)
         result.append( KMimeType::Ptr( newMimeType ) );
   }
   return result;
}

KServiceType::List KServiceTypeFactory::allServiceTypes()
{
   KServiceType::List result;
   KSycocaEntry::List list = allEntries();
   for( KSycocaEntry::List::Iterator it = list.begin();
        it != list.end();
        ++it)
   {
#ifndef Q_WS_QWS
      KServiceType *newServiceType = dynamic_cast<KServiceType *>((*it).data());
#else //FIXME
      KServiceType *newServiceType = (KServiceType*)(*it).data();
#endif
      if (newServiceType)
         result.append( KServiceType::Ptr( newServiceType ) );
   }
   return result;
}

bool KServiceTypeFactory::checkMimeTypes()
{
   QDataStream *str = KSycoca::self()->findFactory( factoryId() );
   if (!str) return false;

   // check if there are mimetypes/servicetypes
   return (m_beginEntryOffset != m_endEntryOffset);
}

KServiceType * KServiceTypeFactory::createEntry(int offset)
{
   KServiceType *newEntry = 0;
   KSycocaType type;
   QDataStream *str = KSycoca::self()->findEntry(offset, type);
   if (!str) return 0;

   switch(type)
   {
     case KST_KServiceType:
        newEntry = new KServiceType(*str, offset);
        break;
     case KST_KMimeType:
        newEntry = new KMimeType(*str, offset);
        break;
     case KST_KFolderType:
        newEntry = new KFolderType(*str, offset);
        break;
     case KST_KDEDesktopMimeType:
        newEntry = new KDEDesktopMimeType(*str, offset);
        break;
     case KST_KExecMimeType:
        newEntry = new KExecMimeType(*str, offset);
        break;

     default:
        kdError(7011) << QString("KServiceTypeFactory: unexpected object entry in KSycoca database (type = %1)").arg((int)type) << endl;
        break;
   }
   if (newEntry && !newEntry->isValid())
   {
      kdError(7011) << "KServiceTypeFactory: corrupt object in KSycoca database!\n" << endl;
      delete newEntry;
      newEntry = 0;
   }
   return newEntry;
}

KServiceTypeFactory *KServiceTypeFactory::_self = 0;

void KServiceTypeFactory::virtual_hook( int id, void* data )
{ KSycocaFactory::virtual_hook( id, data ); }

// vim: ts=3 sw=3 et
