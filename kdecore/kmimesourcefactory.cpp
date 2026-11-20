/*
    This file is part of the KDE libraries
    Copyright (C) 1997 Matthias Kalle Dalheimer (kalle@kde.org)

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

#include <qmap.h>
#include <qpixmap.h>

#include <kdebug.h>
#include <kglobal.h>
#include <kinstance.h>
#include <kiconloader.h>

#include "kmimesourcefactory.h"

class KMimeSourceFactoryPrivate
{
public:
  inline KMimeSourceFactoryPrivate (KIconLoader* loader) : m_iconLoader(loader), m_instance(0L) {}
  inline KIconLoader *iconLoader()
  {
    // If we don't have either of these, things are looking grim.
    Q_ASSERT(m_instance || m_iconLoader);

    if (m_iconLoader)
      return m_iconLoader;

    return m_instance->iconLoader();
  }

  KIconLoader *m_iconLoader;
  KInstance *m_instance;
  // SVG icons stored as pixmaps: "<context>|<name>" -> "<size>:<path>"
  QMap<QString, QString> m_svgIcons;
};

KMimeSourceFactory::KMimeSourceFactory (KIconLoader* loader)
  : QMimeSourceFactory (),
	d (new KMimeSourceFactoryPrivate (loader))
{
}

KMimeSourceFactory::~KMimeSourceFactory()
{
  delete d;
}

QString KMimeSourceFactory::makeAbsolute (const QString& absOrRelName, const QString& context) const
{
  QString myName;
  QString myContext;

  const int pos = absOrRelName.find ('|');
  if (pos > -1)
	{
	  myContext = absOrRelName.left (pos);
	  myName = absOrRelName.right (absOrRelName.length() - myContext.length() - 1);
	}

  QString result;
  KIcon::Group group = KIcon::NoGroup;

  if (myContext == "desktop")
	group = KIcon::Desktop;
  else if (myContext == "toolbar")
	group = KIcon::Toolbar;
  else if (myContext == "maintoolbar")
	group = KIcon::MainToolbar;
  else if (myContext == "small")
	group = KIcon::Small;
  else if (myContext == "user")
	group = KIcon::User;

  if (group != KIcon::NoGroup)
	result = d->iconLoader()->iconPath (myName, group);

  // QMimeSourceFactory reads the file with QImageIO, which cannot read
  // SVG: store the icon rendered by KIconLoader (at the size of its group,
  // "user" icons at their own size) under the name asked for instead.
  const QString ext = result.right (4).lower();
  if (ext == ".svg" || ext == "svgz")
	{
	  const int size = group == KIcon::User ? 0
		: d->iconLoader()->currentSize (group);
	  const QString stored = QString::number (size) + ':' + result;
	  QMap<QString, QString>::ConstIterator it = d->m_svgIcons.find (absOrRelName);
	  if (it != d->m_svgIcons.end() && it.data() == stored)
		return absOrRelName;
	  QPixmap pix = d->iconLoader()->loadIcon (result, KIcon::User, size,
						   KIcon::DefaultState, 0, true);
	  if (!pix.isNull())
		{
		  const_cast<KMimeSourceFactory *> (this)->setPixmap (absOrRelName, pix);
		  d->m_svgIcons[absOrRelName] = stored;
		  return absOrRelName;
		}
	}

  if (result.isEmpty())
	result = QMimeSourceFactory::makeAbsolute (absOrRelName, context);
  
  return result;
}

void KMimeSourceFactory::setInstance(KInstance *instance)
{
  d->m_instance = instance;
}

void KMimeSourceFactory::virtual_hook( int, void* )
{ /*BASE::virtual_hook( id, data );*/ }

