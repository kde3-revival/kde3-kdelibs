/* This file is part of the KDE project
   Copyright (C) 1999 David Faure <faure@kde.org>
 
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

#ifndef __k_build_service_type_factory_h__
#define __k_build_service_type_factory_h__

#include <kservicetypefactory.h>
#include <kmimetype.h>
#include <qstringlist.h>
#include <qmap.h>
#include <qvaluelist.h>

/**
 * Service-type factory for building ksycoca
 * @internal
 */
class KBuildServiceTypeFactory : public KServiceTypeFactory
{
public:
  /**
   * Create factory
   */
  KBuildServiceTypeFactory();
  
  virtual ~KBuildServiceTypeFactory();

  /**
   * Find a service type in the database file 
   * @return a pointer to the servicetype in the memory dict (don't free!)
   */
  virtual KServiceType * findServiceTypeByName(const QString &_name);

  /**
   * Construct a KServiceType from a config file.
   */
  virtual KSycocaEntry * createEntry(const QString &file, const char *resource);

  virtual KServiceType * createEntry( int ) { assert(0); return 0L; }

  /**
   * Add entry 
   */
  virtual void addEntry(KSycocaEntry *newEntry, const char *resource);

  /**
   * Write out service type specific index files.
   */
  virtual void save(QDataStream &str);

  /**
   * Write out header information
   *
   * Don't forget to call the parent first when you override 
   * this function.
   */
  virtual void saveHeader(QDataStream &str);

  /**
   * Returns all resource types for this service factory
   */  
  static QStringList resourceTypes();
private:

  void savePatternLists(QDataStream &str);
  void saveAliases(QDataStream &str);

  /**
   * Merges the shared-mime-info types into the KDE ones: decides which of
   * them become entries of their own and which are aliases of a KDE type,
   * resolves their parents and assigns their glob patterns.
   * Runs once, before the first lookup or the save.
   */
  void mergeSharedMimeInfo();

  // shared-mime-info entries, merged into the factory by mergeSharedMimeInfo()
  QValueList<KMimeType::Ptr> m_smiEntries;
  bool m_smiMerged;
  // alias -> name of the KDE or shared-mime-info type it stands for
  QMap<QString,QString> m_aliases;
  // glob patterns of shared-mime-info types that no KDE type claims
  QMap<QString,KMimeType*> m_smiPatterns;
  QMap<QString,bool> m_smiNoCasePatterns;
  int m_aliasOffset;
};

#endif
