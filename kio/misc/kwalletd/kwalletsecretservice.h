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
#ifndef _KWALLETSECRETSERVICE_H_
#define _KWALLETSECRETSERVICE_H_

#include <qmap.h>

#include "kwalletstore.h"

// @Private
// A wallet kept in the freedesktop.org Secret Service (gnome-keyring,
// KeePassXC, ...), through libsecret.  The service, not KDE, owns the
// encryption and the password: the GNOME login keyring is unlocked by
// the display manager's PAM module (pam_gnome_keyring), so opening the
// wallet does not ask for a password.
//
// Every entry is one item of the service's default collection, with the
// schema "org.kde.kwallet3" and the attributes
//   xdg:schema = org.kde.kwallet3   (added by libsecret)
//   wallet     = <wallet name>      ("kdewallet", ...)
//   folder     = <folder>
//   key        = <entry key>
//   type       = password | map | stream | binary | folder
// and the label "KWallet: <folder>/<key>" ("KWallet (<wallet>): ..." for
// a wallet other than kdewallet).  kwalletd only ever searches, reads and
// deletes items with this schema and the wallet attribute, never the items
// of other applications.
//
// The secrets:
//   password  the password, UTF-8, content type "text/plain"
//   map       KWallet's own encoding of the QMap<QString,QString> (a
//             QDataStream, as the client sends it), content type
//             "application/x-kde-kwallet-map"; lossless and what
//             Wallet::readMap() expects
//   stream    the bytes, "application/octet-stream"
//   binary    a password entry whose bytes are not a QDataStream'd
//             QString (written through writeEntry(..., Password)): the
//             bytes, "application/x-kde-kwallet-entry", kept as they are
//   folder    an empty secret, key "": marks a folder (an empty folder
//             must exist too)
//
// open() loads the whole wallet (searching unlocks a locked collection
// through the service's own prompt); every change is written through at
// once.  Changes made by other programs (Seahorse) are seen the next
// time the wallet is opened.
class KWalletSecretServiceStore : public KWalletStore {
	public:
		KWalletSecretServiceStore(const QString& name);
		virtual ~KWalletSecretServiceStore();

		// Connects to the service, unlocks the default collection if
		// needed (the service prompts) and loads the wallet.  0 on
		// success, < 0 on error.
		int open();

		// Is org.freedesktop.secrets on the session bus (running or
		// activatable)?
		static bool serviceAvailable();
		// The names of the wallets that have items in the service.
		static QStringList walletNames();
		// Removes every item of this wallet.  true if there were any.
		static bool deleteWallet(const QString& wallet);
		// Lookups by attribute, without unlocking or reading secrets.
		static bool folderExists(const QString& wallet, const QString& folder);
		static bool entryExists(const QString& wallet, const QString& folder, const QString& key);

		virtual bool isSecretService() const { return true; }
		virtual const QString& walletName() const { return _name; }
		virtual bool isOpen() const { return _open; }
		virtual int open(const QByteArray&) { return open(); }
		virtual int sync(const QByteArray& password);
		virtual int close(const QByteArray& password);
		virtual QStringList folderList() const;
		virtual bool hasFolder(const QString& f) const;
		virtual bool createFolder(const QString& f);
		virtual bool removeFolder(const QString& f);
		virtual void setFolder(const QString& f) { _folder = f; }
		virtual QStringList entryList() const;
		virtual KWallet::Entry *readEntry(const QString& key);
		virtual QPtrList<KWallet::Entry> readEntryList(const QString& key);
		virtual bool writeEntry(KWallet::Entry *e);
		virtual bool hasEntry(const QString& key) const;
		virtual bool removeEntry(const QString& key);
		virtual int renameEntry(const QString& oldName, const QString& newName);
		virtual bool folderDoesNotExist(const QString& folder) const;
		virtual bool entryDoesNotExist(const QString& folder, const QString& entry) const;

	private:
		void clear();
		bool store(const QString& folder, const KWallet::Entry *e);
		bool erase(const QString& folder, const QString& key);

		typedef QMap<QString, KWallet::Entry*> EntryMap;
		typedef QMap<QString, EntryMap> FolderMap;

		QString _name;
		QString _folder;
		bool _open;
		FolderMap _entries;
};

#endif
