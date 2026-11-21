/*
 *
 *  This file is part of the KDE libraries
 *  Copyright (c) 2001 Waldo Bastian <bastian@kde.org>
 *
 * $Id: autostart.cpp 534738 2006-04-27 18:04:45Z lunakl $
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License version 2 as published by the Free Software Foundation.
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

#include "autostart.h"

#include <kconfig.h>
#include <kdesktopfile.h>
#include <kglobal.h>
#include <kstandarddirs.h>

#include <qdir.h>

class AutoStartItem
{
public:
   QString name;
   QString service;
   QString startAfter;
   int     phase;
};

class AutoStartList: public QPtrList<AutoStartItem>
{
public:
   AutoStartList() { }
};

AutoStart::AutoStart( bool new_startup )
  : m_newStartup( new_startup ), m_phase( new_startup ? -1 : 0), m_phasedone(false)
{
  m_startList = new AutoStartList;
  m_startList->setAutoDelete(true);
  KGlobal::dirs()->addResourceType("autostart", "share/autostart");
  // The autostart directories of the XDG Autostart specification:
  // $XDG_CONFIG_HOME/autostart and each $XDG_CONFIG_DIRS/autostart.
  KGlobal::dirs()->addResourceType("xdgconf-autostart", "autostart/");
}

AutoStart::~AutoStart()
{
	delete m_startList;
}

void
AutoStart::setPhase(int phase)
{
   if (phase > m_phase)
   {
      m_phase = phase;
      m_phasedone = false;
   }
}

void AutoStart::setPhaseDone()
{
   m_phasedone = true;
}

static QString extractName(QString path)
{
  int i = path.findRev('/');
  if (i >= 0)
     path = path.mid(i+1);
  i = path.findRev('.');
  if (i >= 0)
     path = path.left(i);
  return path;
}

static bool startCondition(const QString &condition)
{
  if (condition.isEmpty())
     return true;

  QStringList list = QStringList::split(':', condition, true);
  if (list.count() < 4) 
     return true;
  if (list[0].isEmpty() || list[2].isEmpty()) 
     return true;

  KConfig config(list[0], true, false);
  if (!list[1].isEmpty())
     config.setGroup(list[1]);

  bool defaultValue = (list[3].lower() == "true");

  return config.readBoolEntry(list[2], defaultValue);
}

// The autostart directories, the most important first: the user's XDG
// directory, the user's KDE directory, then the system XDG directories
// ($XDG_CONFIG_DIRS, and the one of the KDE prefix) and the share/autostart
// directories of the KDE prefixes.
static QStringList autoStartDirs()
{
   KStandardDirs *dirs = KGlobal::dirs();
   QStringList xdgDirs = dirs->resourceDirs("xdgconf-autostart");
   QStringList kdeDirs = dirs->resourceDirs("autostart");
   const QString localXdg = KStandardDirs::realPath(dirs->localxdgconfdir() + "autostart/");
   const QString localKde = KStandardDirs::realPath(dirs->localkdedir() + "share/autostart/");

   QStringList result;
   if (xdgDirs.remove(localXdg))
      result.append(localXdg);
   if (kdeDirs.remove(localKde) && !result.contains(localKde))
      result.append(localKde);
   QStringList rest = xdgDirs + kdeDirs;
   for (QStringList::ConstIterator it = rest.begin(); it != rest.end(); ++it)
   {
      if (!result.contains(*it))
         result.append(*it);
   }
   return result;
}

// The desktop files to autostart. A file hides the files of the same name
// in the less important directories, also when it is not started itself
// (Hidden=true in ~/.config/autostart disables a system-wide entry).
static QStringList autoStartFiles()
{
   QStringList files;
   QStringList names;
   QStringList dirs = autoStartDirs();
   for (QStringList::ConstIterator dit = dirs.begin(); dit != dirs.end(); ++dit)
   {
      QDir dir(*dit, "*.desktop", QDir::Name, QDir::Files | QDir::Readable);
      QStringList entries = dir.entryList();
      for (QStringList::ConstIterator it = entries.begin(); it != entries.end(); ++it)
      {
         if (names.contains(*it))
            continue;
         names.append(*it);
         files.append(dir.absFilePath(*it));
      }
   }
   return files;
}

// Our desktop names in XDG_CURRENT_DESKTOP: the session sets
// XDG_CURRENT_DESKTOP=KDE3:GNOME, and older KDE 3 entries say "KDE".
static bool isOurDesktop(const QStringList &desktops)
{
   return desktops.contains("KDE") || desktops.contains("KDE3");
}

void
AutoStart::loadAutoStartList()
{
   QStringList files = autoStartFiles();

   for(QStringList::ConstIterator it = files.begin();
       it != files.end();
       ++it)
   {
       KDesktopFile config(*it, true);
       if (config.readBoolEntry("Hidden", false))
          continue;
       if (!startCondition(config.readEntry("X-KDE-autostart-condition")))
          continue;
       // GNOME's way to switch an entry off.
       if (!config.readBoolEntry("X-GNOME-Autostart-enabled", true))
          continue;
       if (!config.tryExec())
          continue;

       // The session lists GNOME in XDG_CURRENT_DESKTOP too, as a fallback
       // for applications (Chromium and Electron find the GNOME keyring
       // through it, xdg-utils take the GNOME path), but GNOME-only entries
       // are not started: they are the parts of the GNOME desktop itself
       // (the gsd-* settings daemons, gnome-software, gnome-initial-setup,
       // ...) and would fight KDE over the settings, the keys and the
       // screen. Entries without OnlyShowIn are started.
       if (config.hasKey("OnlyShowIn"))
       {
          if (!isOurDesktop(config.readListEntry("OnlyShowIn", ';')))
              continue;
       }
       if (config.hasKey("NotShowIn"))
       {
           if (isOurDesktop(config.readListEntry("NotShowIn", ';')))
               continue;
       }

       AutoStartItem *item = new AutoStartItem;
       item->name = extractName(*it);
       item->service = *it;
       item->startAfter = config.readEntry("X-KDE-autostart-after");
       if( m_newStartup )
       {
          item->phase = config.readNumEntry("X-KDE-autostart-phase", 2);
          if (item->phase < 0)
             item->phase = 0;
       }
       else
       {
          item->phase = config.readNumEntry("X-KDE-autostart-phase", 1);
          if (item->phase < 1)
             item->phase = 1;
       }
       m_startList->append(item);
   }
}

QString
AutoStart::startService()
{
   if (m_startList->isEmpty())
      return 0;

   while(!m_started.isEmpty())
   {

     // Check for items that depend on previously started items
     QString lastItem = m_started[0];
     for(AutoStartItem *item = m_startList->first(); 
         item; item = m_startList->next())
     {
        if (item->phase == m_phase
        &&  item->startAfter == lastItem)
        {
           m_started.prepend(item->name);
           QString service = item->service;
           m_startList->remove();
           return service;
        }
     }
     m_started.remove(m_started.begin());
   }
   
   // Check for items that don't depend on anything
   AutoStartItem *item;
   for(item = m_startList->first();
       item; item = m_startList->next())
   {
      if (item->phase == m_phase
      &&  item->startAfter.isEmpty())
      {
         m_started.prepend(item->name);
         QString service = item->service;
         m_startList->remove();
         return service;
      }
   }

   // Just start something in this phase
   for(item = m_startList->first();
       item; item = m_startList->next())
   {
      if (item->phase == m_phase)
      {
         m_started.prepend(item->name);
         QString service = item->service;
         m_startList->remove();
         return service;
      }
   }

   return 0;
}
