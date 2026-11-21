/*  This file is part of the KDE libraries
 *  Copyright (C) 1999 David Faure   <faure@kde.org>
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

#include "kbuildservicetypefactory.h"
#include "ksharedmimeinfo.h"
#include "ksycoca.h"
#include "ksycocadict.h"
#include "kresourcelist.h"

#include <kglobal.h>
#include <kstandarddirs.h>
#include <qfile.h>
#include <kmessageboxwrapper.h>
#include <kdebug.h>
#include <klocale.h>
#include <assert.h>
#include <kdesktopfile.h>
#include <kstandarddirs.h>

template class QDict<KMimeType>;

// Property keys of the types read from shared-mime-info
static const char s_smiKey[] = "X-KDE-SharedMimeInfo";
static const char s_smiAliasesKey[] = "X-KDE-SharedMimeInfo-Aliases";
static const char s_smiSubClassOfKey[] = "X-KDE-SharedMimeInfo-SubClassOf";
static const char s_smiGlobsKey[] = "X-KDE-SharedMimeInfo-Globs";
static const char s_smiIconKey[] = "X-KDE-SharedMimeInfo-Icon";
static const char s_smiResource[] = "xdgdata-mime";

namespace {
// Gives the factory the properties of a KServiceType it builds
class PropertyAccess : public KMimeType
{
public:
   static QMap<QString,QVariant> &props( KServiceType *t )
   { return t->*( &PropertyAccess::m_mapProps ); }
   static QString &iconOf( KServiceType *t )
   { return t->*( &PropertyAccess::m_strIcon ); }
};
}

static void setProperty( KServiceType *t, const QString &key, const QVariant &value )
{
   if ( value.isValid() )
      PropertyAccess::props( t ).replace( key, value );
   else
      PropertyAccess::props( t ).remove( key );
}

static bool isSharedMimeInfoType( KSycocaEntry *e )
{
   return e->isType( KST_KMimeType ) &&
      static_cast<KMimeType *>( e )->property( s_smiKey ).toBool();
}

KBuildServiceTypeFactory::KBuildServiceTypeFactory() :
  KServiceTypeFactory(), m_smiMerged( false ), m_aliasOffset( 0 )
{
   // Read servicetypes first, since they might be needed to read mimetype properties
   m_resourceList = new KSycocaResourceList;
   m_resourceList->add("servicetypes", "*.desktop");
   m_resourceList->add("servicetypes", "*.kdelnk");
   m_resourceList->add( "mime", "*.desktop" );
   m_resourceList->add( "mime", "*.kdelnk" );
   // freedesktop.org shared-mime-info, after the KDE types: those win
   m_resourceList->add( s_smiResource, "*.xml" );
}

// return all service types for this factory
// i.e. first arguments to m_resourceList->add() above
QStringList KBuildServiceTypeFactory::resourceTypes()
{
    return QStringList() << "servicetypes" << "mime" << s_smiResource;
}

KBuildServiceTypeFactory::~KBuildServiceTypeFactory()
{
   delete m_resourceList;
}

KServiceType * KBuildServiceTypeFactory::findServiceTypeByName(const QString &_name)
{
   assert (KSycoca::self()->isBuilding());
   if ( !m_smiMerged && !m_smiEntries.isEmpty() )
      mergeSharedMimeInfo();
   // We're building a database - the service type must be in memory
   KSycocaEntry::Ptr * servType = (*m_entryDict)[ _name ];
   if (!servType)
   {
      QMap<QString,QString>::ConstIterator alias = m_aliases.find( _name );
      if ( alias != m_aliases.end() )
         servType = (*m_entryDict)[ alias.data() ];
   }
   if (!servType)
      return 0;
   return (KServiceType *) ((KSycocaEntry*)*servType);
}

static KSycocaEntry *createSharedMimeInfoEntry( const QString &file )
{
   // <media>/<subtype>.xml; "packages/*.xml" are the sources the per-type
   // files are generated from
   if ( file.contains( '/' ) != 1 || file.startsWith( "packages/" ) )
      return 0;
   // Every directory's copy, most important first: xdgmime merges them
   // (a package in ~/.local/share/mime adding a glob to a system type)
   const QStringList dirs = KGlobal::dirs()->resourceDirs( s_smiResource );
   const QStringList languages = KGlobal::locale()->languageList();
   KSharedMimeInfoType type;
   bool found = false;
   for ( QStringList::ConstIterator dir = dirs.begin(); dir != dirs.end(); ++dir )
   {
      const QString path = *dir + file;
      if ( !QFile::exists( path ) )
         continue;
      KSharedMimeInfoType copy;
      if ( !KSharedMimeInfo::parseTypeFile( path, languages, copy ) )
         continue;
      // The file name is the type name (update-mime-database lowercases
      // it): one entry per name, like mimelnk
      if ( ( copy.name + ".xml" ).lower() != file.lower() )
      {
         kdWarning(7012) << "MIME type file " << path << " defines " << copy.name << endl;
         continue;
      }
      if ( !found )
         type = copy;
      else if ( copy.name == type.name )
         KSharedMimeInfo::merge( type, copy );
      found = true;
   }
   if ( !found )
      return 0;

   KMimeType *e = new KMimeType( file, type.name, KSharedMimeInfo::chooseIcon( type ),
                                 type.comment, type.patterns );
   setProperty( e, s_smiKey, QVariant( true, 0 ) );
   // the icon of the type itself; mergeSharedMimeInfo() may use its parent's
   setProperty( e, s_smiIconKey, e->KServiceType::icon() );
   if ( !type.aliases.isEmpty() )
      setProperty( e, s_smiAliasesKey, type.aliases );
   if ( !type.subClassOf.isEmpty() )
      setProperty( e, s_smiSubClassOfKey, type.subClassOf );
   if ( !type.globs.isEmpty() )
      setProperty( e, s_smiGlobsKey, type.globs );
   return e;
}


KSycocaEntry *
KBuildServiceTypeFactory::createEntry(const QString &file, const char *resource)
{
  QString name = file;
  int pos = name.findRev('/');
  if (pos != -1)
  {
     name = name.mid(pos+1);
  }

  if (name.isEmpty())
     return 0;

  if ( strcmp( resource, s_smiResource ) == 0 )
     return createSharedMimeInfoEntry( file );

  KDesktopFile desktopFile(file, true, resource);

  if ( desktopFile.readBoolEntry( "Hidden", false ) == true )
      return 0;

  // TODO check Type field first
  QString mime = desktopFile.readEntry( "MimeType" );
  QString service = desktopFile.readEntry( "X-KDE-ServiceType" );

  if ( mime.isEmpty() && service.isEmpty() )
  {
    QString tmp = QString("The service/mime type config file\n%1\n"
                  "does not contain a ServiceType=...\nor MimeType=... entry").arg( file );
    kdWarning(7012) << tmp << endl;
    return 0;
  }

  KServiceType* e;
  if ( mime == "inode/directory" )
    e = new KFolderType( &desktopFile );
  else if ( mime == "application/x-desktop" )
    e = new KDEDesktopMimeType( &desktopFile );
  else if ( mime == "application/x-executable" || mime == "application/x-shellscript" )
    e = new KExecMimeType( &desktopFile );
  else if ( !mime.isEmpty() )
    e = new KMimeType( &desktopFile );
  else
    e = new KServiceType( &desktopFile );

  if (e->isDeleted())
  {
    delete e;
    return 0;
  }

  if ( !(e->isValid()) )
  {
    kdWarning(7012) << "Invalid ServiceType : " << file << endl;
    delete e;
    return 0;
  }

  return e;
}

void
KBuildServiceTypeFactory::saveHeader(QDataStream &str)
{
   KSycocaFactory::saveHeader(str);
   str << (Q_INT32) m_fastPatternOffset;
   str << (Q_INT32) m_otherPatternOffset;
   str << (Q_INT32) m_propertyTypeDict.count();

   QMapIterator<QString, int> it;
   for (it = m_propertyTypeDict.begin(); it != m_propertyTypeDict.end(); ++it)
   {
     str << it.key() << (Q_INT32)it.data();
   }
   str << (Q_INT32) m_aliasOffset;
}

void
KBuildServiceTypeFactory::save(QDataStream &str)
{
   mergeSharedMimeInfo();

   KSycocaFactory::save(str);

   savePatternLists(str);
   saveAliases(str);

   int endOfFactoryData = str.device()->at();

   // Update header (pass #3)
   saveHeader(str);

   // Seek to end.
   str.device()->at(endOfFactoryData);
}

void
KBuildServiceTypeFactory::savePatternLists(QDataStream &str)
{
   // Store each patterns in one of the 2 string lists (for sorting)
   QStringList fastPatterns;  // for *.a to *.abcd
   QStringList otherPatterns; // for the rest (core.*, *.tar.bz2, *~) ...
   QDict<KMimeType> dict;

   // For each mimetype in servicetypeFactory
   for(QDictIterator<KSycocaEntry::Ptr> it ( *m_entryDict );
       it.current();
       ++it)
   {
      KSycocaEntry *entry = (*it.current());
      // The patterns of the shared-mime-info types are added below, where
      // no KDE type claims them
      if ( entry->isType( KST_KMimeType ) && !isSharedMimeInfoType( entry ) )
      {
        KMimeType *mimeType = (KMimeType *) entry;
        QStringList pat = mimeType->patterns();
        QStringList::ConstIterator patit = pat.begin();
        for ( ; patit != pat.end() ; ++patit )
        {
           const QString &pattern = *patit;
           if ( pattern.findRev('*') == 0
                && pattern.findRev('.') == 1
                && pattern.length() <= 6 )
              // it starts with "*.", has no other '*' and no other '.', and is max 6 chars
              // => fast patttern
              fastPatterns.append( pattern );
           else if (!pattern.isEmpty()) // some stupid mimetype files have "Patterns=;"
              otherPatterns.append( pattern );
           // Assumption : there is only one mimetype for that pattern
           // It doesn't really make sense otherwise, anyway.
           dict.replace( pattern, mimeType );
        }
      }
   }
   for ( QMap<QString,KMimeType*>::ConstIterator smit = m_smiPatterns.begin();
         smit != m_smiPatterns.end(); ++smit )
   {
      const QString &pattern = smit.key();
      if ( dict.find( pattern ) )
         continue;
      if ( pattern.findRev('*') == 0 && pattern.findRev('.') == 1 && pattern.length() <= 6 &&
           pattern.find('?') == -1 && pattern.find('[') == -1 )
         fastPatterns.append( pattern );
      else
         otherPatterns.append( pattern );
      dict.replace( pattern, smit.data() );
   }
   // Sort the list - the fast one, useless for the other one
   fastPatterns.sort();

   Q_INT32 entrySize = 0;
   Q_INT32 nrOfEntries = 0;

   m_fastPatternOffset = str.device()->at();

   // Write out fastPatternHeader (Pass #1)
   str.device()->at(m_fastPatternOffset);
   str << nrOfEntries;
   str << entrySize;

   // For each fast pattern
   QStringList::ConstIterator it = fastPatterns.begin();
   for ( ; it != fastPatterns.end() ; ++it )
   {
     int start = str.device()->at();
     // Justify to 6 chars with spaces, so that the size remains constant
     // in the database file.
     QString paddedPattern = (*it).leftJustify(6).right(4); // remove leading "*."
     //kdDebug(7021) << QString("FAST : '%1' '%2'").arg(paddedPattern).arg(dict[(*it)]->name()) << endl;
     str << paddedPattern;
     str << dict[(*it)]->offset();
     entrySize = str.device()->at() - start;
     nrOfEntries++;
   }

   // store position
   m_otherPatternOffset = str.device()->at();

   // Write out fastPatternHeader (Pass #2)
   str.device()->at(m_fastPatternOffset);
   str << nrOfEntries;
   str << entrySize;

   // For the other patterns
   str.device()->at(m_otherPatternOffset);

   it = otherPatterns.begin();
   for ( ; it != otherPatterns.end() ; ++it )
   {
     //kdDebug(7021) << QString("OTHER : '%1' '%2'").arg(*it).arg(dict[(*it)]->name()) << endl;
     str << (*it);
     str << dict[(*it)]->offset();
   }

   str << QString(""); // end of list marker (has to be a string !)
}

void
KBuildServiceTypeFactory::saveAliases(QDataStream &str)
{
   m_aliasOffset = str.device()->at();
   QMap<QString,QString> aliases;
   for ( QMap<QString,QString>::ConstIterator it = m_aliases.begin(); it != m_aliases.end(); ++it )
      if ( !(*m_entryDict)[ it.key() ] && (*m_entryDict)[ it.data() ] )
         aliases.insert( it.key(), it.data() );
   str << (Q_INT32) aliases.count();
   for ( QMap<QString,QString>::ConstIterator it = aliases.begin(); it != aliases.end(); ++it )
      str << it.key() << it.data();

   // The patterns the lowercased file name is matched against: those of
   // shared-mime-info that are not case-sensitive (not KDE's)
   QStringList noCase;
   for ( QMap<QString,KMimeType*>::ConstIterator it = m_smiPatterns.begin(); it != m_smiPatterns.end(); ++it )
      if ( m_smiNoCasePatterns.contains( it.key() ) )
         noCase.append( it.key() );
   str << (Q_INT32) noCase.count();
   for ( QStringList::ConstIterator it = noCase.begin(); it != noCase.end(); ++it )
      str << *it;
}

void
KBuildServiceTypeFactory::addEntry(KSycocaEntry *newEntry, const char *resource)
{
   if ( resource && strcmp( resource, s_smiResource ) == 0 )
   {
      // Kept aside until all the KDE types are known
      if ( !newEntry->isType( KST_KMimeType ) )
         return;
      if ( m_smiMerged )
         kdWarning(7021) << "Shared MIME type " << newEntry->name() << " added too late" << endl;
      else
         m_smiEntries.append( KMimeType::Ptr( static_cast<KMimeType *>( newEntry ) ) );
      return;
   }

   KServiceType * serviceType = (KServiceType *) newEntry;
   if ( (*m_entryDict)[ newEntry->name() ] )
   {
     // Already exists
     if (serviceType->desktopEntryPath().endsWith("kdelnk"))
        return; // Skip
     
     // Replace
     KSycocaFactory::removeEntry(newEntry);
   }
   KSycocaFactory::addEntry(newEntry, resource);


   const QMap<QString,QVariant::Type>& pd = serviceType->propertyDefs();
   QMap<QString,QVariant::Type>::ConstIterator pit = pd.begin();
   for( ; pit != pd.end(); ++pit )
   {
     if (!m_propertyTypeDict.contains(pit.key()))
       m_propertyTypeDict.insert(pit.key(), pit.data());
     else if (m_propertyTypeDict[pit.key()] != pit.data())
       kdWarning(7021) << "Property '"<< pit.key() << "' is defined multiple times ("<< serviceType->name() <<")" <<endl;
   }
}


namespace {

struct MergeContext
{
   MergeContext() : kde( 1009 ), kdePatterns( 4001 ) {}
   QDict<KMimeType> kde;                // the KDE mime types
   QDict<KMimeType> kdePatterns;        // lowercased KDE pattern -> type
   QMap<QString,KMimeType*> smi;        // shared-mime-info types
   QMap<QString,QString> smiAliasOf;    // shared-mime-info alias -> type
   QMap<QString,QString> target;        // resolved names
};

QString mediaOf( const QString &name )
{
   return name.left( name.find( '/' ) );
}

// shared-mime-info types KDE 3 has under another name, without
// shared-mime-info knowing that name as an alias
static const char * const s_kdeNames[][2] = {
   { "application/x-compressed-tar", "application/x-tgz" },
   { "application/x-bzip-compressed-tar", "application/x-tbz" },
   { "application/x-bzip2-compressed-tar", "application/x-tbz" },
   { "application/x-lzop-compressed-tar", "application/x-tzo" },
   { "application/x-7z-compressed", "application/x-7z" },
   { "application/vnd.rar", "application/x-rar" },
   { "application/vnd.debian.binary-package", "application/x-deb" },
   { "audio/mpeg", "audio/x-mp3" },
   { "audio/flac", "audio/x-flac" },
   { "image/x-xcf", "image/x-xcf-gimp" },
   { "text/x-patch", "text/x-diff" },
   { 0, 0 }
};

QString kdeName( MergeContext &ctx, const QString &name )
{
   for ( int i = 0; s_kdeNames[i][0]; ++i )
      if ( name == s_kdeNames[i][0] && ctx.kde.find( s_kdeNames[i][1] ) )
         return QString::fromLatin1( s_kdeNames[i][1] );
   return QString::null;
}

// The name under which the type @p name is in the database: a KDE type,
// a shared-mime-info type of its own, or the KDE type it is merged into.
QString resolveTarget( MergeContext &ctx, const QString &name, int depth = 0 )
{
   if ( ctx.kde.find( name ) )
      return name;
   QMap<QString,QString>::ConstIterator cached = ctx.target.find( name );
   if ( cached != ctx.target.end() )
      return cached.data();
   if ( depth > 8 )
      return QString::null;

   QString result;
   // shared-mime-info declares the name an alias of another type
   // (application/x-gerber is application/vnd.gerber even though KiCad
   // also defines it): the alias wins, as in xdgmime
   QMap<QString,QString>::ConstIterator aliasOf = ctx.smiAliasOf.find( name );
   if ( aliasOf != ctx.smiAliasOf.end() && ctx.smi.contains( aliasOf.data() ) )
      result = resolveTarget( ctx, aliasOf.data(), depth + 1 );

   QMap<QString,KMimeType*>::ConstIterator s = ctx.smi.find( name );
   if ( result.isEmpty() && s != ctx.smi.end() )
   {
      // KDE knows the type under one of its aliases (application/zip is
      // KDE's application/x-zip): the KDE type stays the real one
      // (if it knows several, the one with its patterns: application/x-zip
      // rather than application/x-zip-compressed)
      const QStringList aliases = s.data()->property( s_smiAliasesKey ).toStringList();
      const QStringList &patterns = s.data()->patterns();
      for ( QStringList::ConstIterator it = aliases.begin(); it != aliases.end(); ++it )
      {
         KMimeType *k = ctx.kde.find( *it );
         if ( !k )
            continue;
         if ( result.isEmpty() )
            result = *it;
         bool owner = false;
         for ( QStringList::ConstIterator p = patterns.begin(); p != patterns.end() && !owner; ++p )
            owner = ctx.kdePatterns.find( (*p).lower() ) == k;
         if ( owner )
         {
            result = *it;
            break;
         }
      }
      if ( result.isEmpty() )
         result = kdeName( ctx, name );
      if ( result.isEmpty() )
         result = name;
   }
   if ( !result.isEmpty() )
      ctx.target.insert( name, result );
   return result;
}

}

void KBuildServiceTypeFactory::mergeSharedMimeInfo()
{
   if ( m_smiMerged )
      return;
   m_smiMerged = true;
   if ( m_smiEntries.isEmpty() )
      return;

   MergeContext ctx;
   for ( QDictIterator<KSycocaEntry::Ptr> it( *m_entryDict ); it.current(); ++it )
   {
      KSycocaEntry *e = *it.current();
      if ( !e->isType( KST_KMimeType ) )
         continue;
      KMimeType *m = static_cast<KMimeType *>( e );
      ctx.kde.insert( m->name(), m );
      const QStringList &patterns = m->patterns();
      for ( QStringList::ConstIterator p = patterns.begin(); p != patterns.end(); ++p )
         if ( !(*p).isEmpty() )
            ctx.kdePatterns.replace( (*p).lower(), m );
   }

   for ( QValueList<KMimeType::Ptr>::ConstIterator it = m_smiEntries.begin();
         it != m_smiEntries.end(); ++it )
   {
      KMimeType *m = *it;
      if ( !ctx.smi.contains( m->name() ) )
         ctx.smi.insert( m->name(), m );
   }
   for ( QMap<QString,KMimeType*>::ConstIterator it = ctx.smi.begin(); it != ctx.smi.end(); ++it )
   {
      const QStringList aliases = it.data()->property( s_smiAliasesKey ).toStringList();
      for ( QStringList::ConstIterator a = aliases.begin(); a != aliases.end(); ++a )
         if ( *a != it.key() && !ctx.smiAliasOf.contains( *a ) )
            ctx.smiAliasOf.insert( *a, it.key() );
   }

   // Which types become entries of their own
   QMap<QString,KMimeType*> own;
   for ( QMap<QString,KMimeType*>::ConstIterator it = ctx.smi.begin(); it != ctx.smi.end(); ++it )
      if ( !ctx.kde.find( it.key() ) && resolveTarget( ctx, it.key() ) == it.key() )
         own.insert( it.key(), it.data() );

   // Aliases: of the types merged into another one, and the declared ones
   for ( QMap<QString,KMimeType*>::ConstIterator it = ctx.smi.begin(); it != ctx.smi.end(); ++it )
   {
      const QString target = resolveTarget( ctx, it.key() );
      if ( target.isEmpty() )
         continue;
      if ( target != it.key() && !ctx.kde.find( it.key() ) && !m_aliases.contains( it.key() ) )
         m_aliases.insert( it.key(), target );
      const QStringList aliases = it.data()->property( s_smiAliasesKey ).toStringList();
      for ( QStringList::ConstIterator a = aliases.begin(); a != aliases.end(); ++a )
         if ( *a != target && !ctx.kde.find( *a ) && !own.contains( *a ) && !m_aliases.contains( *a ) )
            m_aliases.insert( *a, target );
   }

   // MIME type names are case-insensitive: audio/AMR-WB is also audio/amr-wb
   for ( QMap<QString,KMimeType*>::ConstIterator it = ctx.smi.begin(); it != ctx.smi.end(); ++it )
   {
      const QString target = resolveTarget( ctx, it.key() );
      const QString lower = it.key().lower();
      if ( lower != it.key() && !target.isEmpty() && !ctx.kde.find( lower ) &&
           !own.contains( lower ) && !m_aliases.contains( lower ) )
         m_aliases.insert( lower, target );
   }

   // Parents, resolved to the names in the database
   QMap<QString,QStringList> parents;
   for ( QMap<QString,KMimeType*>::ConstIterator it = own.begin(); it != own.end(); ++it )
   {
      QStringList declared = it.data()->property( s_smiSubClassOfKey ).toStringList();
      // every text/* type is a text/plain
      if ( declared.isEmpty() && mediaOf( it.key() ) == "text" && it.key() != "text/plain" )
         declared.append( "text/plain" );
      QStringList resolved;
      for ( QStringList::ConstIterator p = declared.begin(); p != declared.end(); ++p )
      {
         QString r;
         if ( ctx.kde.find( *p ) || own.contains( *p ) )
            r = *p;
         else if ( m_aliases.contains( *p ) )
            r = m_aliases[ *p ];
         if ( !r.isEmpty() && r != it.key() && !resolved.contains( r ) )
            resolved.append( r );
      }
      parents.insert( it.key(), resolved );
   }

   // Drop the parents that would make a loop with the KDE "IsAlso" chains
   // or the other shared-mime-info types: KMimeType::is() and the offers
   // walk these graphs
   for ( QMap<QString,QStringList>::Iterator it = parents.begin(); it != parents.end(); ++it )
   {
      QStringList kept;
      for ( QStringList::ConstIterator p = it.data().begin(); p != it.data().end(); ++p )
      {
         bool loops = false;
         QStringList todo( *p );
         QMap<QString,bool> seen;
         while ( !todo.isEmpty() && !loops && seen.count() < 10000 )
         {
            const QString n = todo.first();
            todo.pop_front();
            if ( n == it.key() )
            {
               loops = true;
               break;
            }
            if ( seen.contains( n ) )
               continue;
            seen.insert( n, true );
            if ( parents.contains( n ) )
               todo += parents[ n ];
            else if ( KMimeType *k = ctx.kde.find( n ) )
            {
               QString up = k->parentMimeType();
               if ( !up.isEmpty() && !ctx.kde.find( up ) && m_aliases.contains( up ) )
                  up = m_aliases[ up ];
               if ( !up.isEmpty() )
                  todo.append( up );
            }
         }
         if ( loops )
            kdWarning(7021) << "Ignoring " << it.key() << " as a subclass of " << *p
                            << ": it would be its own parent" << endl;
         else
            kept.append( *p );
      }
      it.data() = kept;
   }

   // Glob patterns: the KDE types keep theirs; among the shared-mime-info
   // types the highest weight wins
   struct Candidate { KMimeType *type; int weight; bool ambiguous; };
   QMap<QString,Candidate> candidates;
   for ( QMap<QString,KMimeType*>::ConstIterator it = ctx.smi.begin(); it != ctx.smi.end(); ++it )
   {
      const QString target = resolveTarget( ctx, it.key() );
      KMimeType *t = ctx.kde.find( target );
      if ( !t )
         t = own.contains( target ) ? own[ target ] : 0;
      if ( !t )
         continue;
      const QStringList globs = it.data()->property( s_smiGlobsKey ).toStringList();
      for ( QStringList::ConstIterator g = globs.begin(); g != globs.end(); ++g )
      {
         const int c1 = (*g).find( ':' );
         const int c2 = (*g).find( ':', c1 + 1 );
         if ( c1 < 0 || c2 < 0 )
            continue;
         const int weight = (*g).left( c1 ).toInt();
         const QString pattern = (*g).mid( c2 + 1 );
         if ( pattern.isEmpty() || ctx.kdePatterns.find( pattern.lower() ) )
            continue;
         // a pattern that is not case-sensitive also matches lowercased names
         if ( (*g).mid( c1 + 1, c2 - c1 - 1 ) != "cs" )
            m_smiNoCasePatterns.insert( pattern, true );
         QMap<QString,Candidate>::Iterator c = candidates.find( pattern );
         if ( c == candidates.end() )
         {
            Candidate n = { t, weight, false };
            candidates.insert( pattern, n );
         }
         else if ( c.data().type == t )
            c.data().weight = QMAX( c.data().weight, weight );
         else if ( weight > c.data().weight )
         {
            Candidate n = { t, weight, false };
            c.data() = n;
         }
         else if ( weight == c.data().weight )
         {
            // Same pattern, same weight: shared-mime-info looks at the
            // contents. One of them gets the pattern, and asks for magic.
            c.data().ambiguous = true;
            if ( t->name() < c.data().type->name() )
               c.data().type = t;
         }
      }
   }
   QMap<QString,bool> ambiguous;
   for ( QMap<QString,Candidate>::ConstIterator c = candidates.begin(); c != candidates.end(); ++c )
   {
      m_smiPatterns.insert( c.key(), c.data().type );
      if ( c.data().ambiguous )
         ambiguous.insert( c.data().type->name(), true );
   }

   // The entries of their own: set their derived properties (a reused
   // entry still has those of the previous build) and add them
   for ( QMap<QString,KMimeType*>::ConstIterator it = own.begin(); it != own.end(); ++it )
   {
      KMimeType *m = it.data();
      const QStringList p = parents[ it.key() ];
      setProperty( m, "X-KDE-IsAlso", p.isEmpty() ? QVariant() : QVariant( p.first() ) );
      setProperty( m, "X-KDE-ParentMimeTypes", p.count() > 1 ? QVariant( p ) : QVariant() );
      setProperty( m, "X-KDE-PatternsAccuracy",
                   ambiguous.contains( it.key() ) ? QVariant( QString::fromLatin1( "50" ) ) : QVariant() );

      // Is it text? (application/x-kicad-pcb is a text/plain)
      bool text = false;
      if ( mediaOf( it.key() ) != "text" )
      {
         QStringList todo = p;
         QMap<QString,bool> seen;
         while ( !todo.isEmpty() && !text && seen.count() < 1000 )
         {
            const QString n = todo.first();
            todo.pop_front();
            if ( seen.contains( n ) )
               continue;
            seen.insert( n, true );
            if ( n == "text/plain" || mediaOf( n ) == "text" )
               text = true;
            else if ( parents.contains( n ) )
               todo += parents[ n ];
            else if ( KMimeType *k = ctx.kde.find( n ) )
            {
               QVariant v = k->property( "X-KDE-text" );
               if ( v.isValid() && v.toBool() )
                  text = true;
               else if ( !k->parentMimeType().isEmpty() )
                  todo.append( k->parentMimeType() );
            }
         }
      }
      setProperty( m, "X-KDE-text", text ? QVariant( true, 0 ) : QVariant() );

      // No icon of its own: that of its nearest ancestor with one (a zip
      // file's for a FreeCAD document) says more than "unknown". From the
      // types' own icons, not the ones set here: the result must not depend
      // on the order, nor on an entry reused from the previous build.
      QString icon = m->property( s_smiIconKey ).toString();
      QString up = p.isEmpty() ? QString::null : p.first();
      for ( int depth = 0; icon == "unknown" && !up.isEmpty() && depth < 32; ++depth )
      {
         if ( KMimeType *k = ctx.kde.find( up ) )
         {
            if ( !k->KServiceType::icon().isEmpty() && k->KServiceType::icon() != "unknown" )
               icon = k->KServiceType::icon();
            break;
         }
         if ( !own.contains( up ) )
            break;
         const QString upIcon = own[ up ]->property( s_smiIconKey ).toString();
         if ( !upIcon.isEmpty() && upIcon != "unknown" )
            icon = upIcon;
         const QStringList &upParents = parents[ up ];
         up = upParents.isEmpty() ? QString::null : upParents.first();
      }
      if ( !icon.isEmpty() )
         PropertyAccess::iconOf( m ) = icon;

      KSycocaFactory::addEntry( m, s_smiResource );
   }

   kdDebug(7021) << "shared-mime-info: " << ctx.smi.count() << " types, " << own.count()
                 << " new, " << m_aliases.count() << " aliases, " << m_smiPatterns.count()
                 << " patterns" << endl;
}
