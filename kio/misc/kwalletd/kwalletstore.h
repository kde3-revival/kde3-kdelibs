/*
   This file is part of the KDE libraries

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
#ifndef _KWALLETSTORE_H_
#define _KWALLETSTORE_H_

#include <qptrlist.h>
#include <qstring.h>
#include <qstringlist.h>

#include "kwalletbackend.h"

// @Private
// An open wallet as kwalletd sees it: either a classic encrypted file
// (KWallet::Backend) or the items of the freedesktop.org Secret Service
// (KWalletSecretServiceStore).  The interface is the subset of
// KWallet::Backend that kwalletd uses; the semantics are the same.
class KWalletStore {
	public:
		KWalletStore() : _ref(0) {}
		virtual ~KWalletStore() {}

		// true for the Secret Service: no password of its own
		virtual bool isSecretService() const = 0;

		virtual const QString& walletName() const = 0;
		virtual bool isOpen() const = 0;

		// Open (the file backend decrypts with the password; the
		// Secret Service does not need it).
		virtual int open(const QByteArray& password) = 0;

		// Save (the file backend encrypts with the password; the
		// Secret Service writes through, these do not need it)
		virtual int sync(const QByteArray& password) = 0;
		// Save and lock.
		virtual int close(const QByteArray& password) = 0;

		virtual QStringList folderList() const = 0;
		virtual bool hasFolder(const QString& f) const = 0;
		virtual bool createFolder(const QString& f) = 0;
		virtual bool removeFolder(const QString& f) = 0;
		virtual void setFolder(const QString& f) = 0;

		virtual QStringList entryList() const = 0;
		virtual KWallet::Entry *readEntry(const QString& key) = 0;
		virtual QPtrList<KWallet::Entry> readEntryList(const QString& key) = 0;
		// false: the entry could not be stored
		virtual bool writeEntry(KWallet::Entry *e) = 0;
		virtual bool hasEntry(const QString& key) const = 0;
		virtual bool removeEntry(const QString& key) = 0;
		virtual int renameEntry(const QString& oldName, const QString& newName) = 0;

		virtual bool folderDoesNotExist(const QString& folder) const = 0;
		virtual bool entryDoesNotExist(const QString& folder, const QString& entry) const = 0;

		int ref() { return ++_ref; }
		int deref() { return --_ref; }
		int refCount() const { return _ref; }

	private:
		int _ref;
};


// The classic wallet: a Blowfish-encrypted file in
// $KDEHOME/share/apps/kwallet (or any path, openPath()).
class KWalletFileStore : public KWalletStore {
	public:
		KWalletFileStore(const QString& name, bool isPath) : _b(name, isPath) {}

		virtual int open(const QByteArray& password) { return _b.open(password); }

		virtual bool isSecretService() const { return false; }
		virtual const QString& walletName() const { return _b.walletName(); }
		virtual bool isOpen() const { return _b.isOpen(); }
		virtual int sync(const QByteArray& password) { return _b.sync(password); }
		virtual int close(const QByteArray& password) { return _b.close(password); }
		virtual QStringList folderList() const { return _b.folderList(); }
		virtual bool hasFolder(const QString& f) const { return _b.hasFolder(f); }
		virtual bool createFolder(const QString& f) { return _b.createFolder(f); }
		virtual bool removeFolder(const QString& f) { return _b.removeFolder(f); }
		virtual void setFolder(const QString& f) { _b.setFolder(f); }
		virtual QStringList entryList() const { return _b.entryList(); }
		virtual KWallet::Entry *readEntry(const QString& key) { return _b.readEntry(key); }
		virtual QPtrList<KWallet::Entry> readEntryList(const QString& key) { return _b.readEntryList(key); }
		virtual bool writeEntry(KWallet::Entry *e) { _b.writeEntry(e); return true; }
		virtual bool hasEntry(const QString& key) const { return _b.hasEntry(key); }
		virtual bool removeEntry(const QString& key) { return _b.removeEntry(key); }
		virtual int renameEntry(const QString& oldName, const QString& newName) { return _b.renameEntry(oldName, newName); }
		virtual bool folderDoesNotExist(const QString& folder) const { return _b.folderDoesNotExist(folder); }
		virtual bool entryDoesNotExist(const QString& folder, const QString& entry) const { return _b.entryDoesNotExist(folder, entry); }

	private:
		KWallet::Backend _b;
};

#endif
