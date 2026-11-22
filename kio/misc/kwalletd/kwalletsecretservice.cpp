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

// libsecret (GLib, GIO) before any Qt header: GIO has struct members
// named "signals", which Qt defines as a macro
#include <libsecret/secret.h>

#include "kwalletsecretservice.h"

#include <kdebug.h>

#include <qdatastream.h>
#include <qregexp.h>

#include <string.h>

// The secret values that kwalletd writes, by entry type
#define CT_TEXT    "text/plain"
#define CT_MAP     "application/x-kde-kwallet-map"
#define CT_STREAM  "application/octet-stream"
#define CT_ENTRY   "application/x-kde-kwallet-entry"

namespace {

const SecretSchema *schema() {
	static SecretSchema *s = 0L;
	if (!s) {
		s = secret_schema_new("org.kde.kwallet3", SECRET_SCHEMA_NONE,
				"wallet", SECRET_SCHEMA_ATTRIBUTE_STRING,
				"folder", SECRET_SCHEMA_ATTRIBUTE_STRING,
				"key", SECRET_SCHEMA_ATTRIBUTE_STRING,
				"type", SECRET_SCHEMA_ATTRIBUTE_STRING,
				NULL);
	}
	return s;
}


enum Scope { WholeWallet, WholeFolder, OneEntry };

// The attributes of a search or an item: the whole wallet, a folder or
// one entry.  A null folder or key is "" (as QMap treats it), never a
// wildcard.
GHashTable *attributes(Scope scope, const QString& wallet, const QString& folder = QString::null,
		const QString& key = QString::null, const char *type = 0L) {
	GHashTable *a = g_hash_table_new_full(g_str_hash, g_str_equal, 0L, g_free);
	g_hash_table_insert(a, const_cast<char*>("wallet"), g_strdup(wallet.utf8().data()));
	if (scope != WholeWallet) {
		g_hash_table_insert(a, const_cast<char*>("folder"), g_strdup(folder.isNull() ? "" : folder.utf8().data()));
	}
	if (scope == OneEntry) {
		g_hash_table_insert(a, const_cast<char*>("key"), g_strdup(key.isNull() ? "" : key.utf8().data()));
	}
	if (type) {
		g_hash_table_insert(a, const_cast<char*>("type"), g_strdup(type));
	}
	return a;
}


void warn(const char *what, GError *err) {
	// the messages of libsecret and of the service, never a secret
	kdWarning() << "kwalletd (Secret Service): " << what << ": "
		<< (err ? err->message : "unknown error") << endl;
	if (err) {
		g_error_free(err);
	}
}


// The service, with an encrypted session (libsecret negotiates
// dh-ietf1024-sha256-aes128-cbc-pkcs7 and falls back to "plain" only
// on a service that offers nothing else).  Null if it is not there.
bool connected = false;

// The service's proxy is made in this context, so that its signals
// (every change to the keyring, by any program) are queued here and
// dropped by drain(), instead of piling up in a default context that a
// Qt event loop may never run.
GMainContext *proxyContext() {
	static GMainContext *c = 0L;
	if (!c) {
		c = g_main_context_new();
	}
	return c;
}

void drain() {
	while (g_main_context_iteration(proxyContext(), FALSE)) {
	}
}

SecretService *service() {
	// libsecret does not cope well with a service that is not there
	// (GLib criticals): look first, until connected
	if (!connected && !KWalletSecretServiceStore::serviceAvailable()) {
		kdWarning() << "kwalletd (Secret Service): org.freedesktop.secrets is not on the session bus" << endl;
		return 0L;
	}
	GError *err = 0L;
	g_main_context_push_thread_default(proxyContext());
	SecretService *s = secret_service_get_sync(SECRET_SERVICE_OPEN_SESSION, 0L, &err);
	g_main_context_pop_thread_default(proxyContext());
	drain();
	if (!s) {
		warn("cannot connect", err);
		return 0L;
	}
	if (err) {
		g_error_free(err);
	}
	connected = true;
	return s;
}


// After an error: forget the connection, the next call makes a new one
// (the service may have exited and been restarted, its session is gone).
void reset(SecretService *s) {
	if (s) {
		g_object_unref(s);
	}
	secret_service_disconnect();
	connected = false;
	drain();
}


void done(SecretService *s) {
	g_object_unref(s);
	drain();
}


QString attribute(GHashTable *a, const char *name) {
	const char *v = static_cast<const char*>(g_hash_table_lookup(a, name));
	return v ? QString::fromUtf8(v) : QString::null;
}


const char *typeName(KWallet::Wallet::EntryType t) {
	switch (t) {
		case KWallet::Wallet::Password: return "password";
		case KWallet::Wallet::Stream: return "stream";
		case KWallet::Wallet::Map: return "map";
		default: return "unknown";
	}
}


// Does the value of this password entry survive the round trip through
// UTF-8 text?  (A password written as raw bytes does not.)
bool isPlainPassword(const KWallet::Entry *e) {
	QString p = e->password();
	if (p.isNull()) {
		return false;
	}
	QByteArray b;
	QDataStream ds(b, IO_WriteOnly);
	ds << p;
	bool rc = (b == e->value());
	b.fill(0);
	p.fill(QChar(0));
	return rc;
}

// The type attribute an entry is stored with.
const char *storedType(const KWallet::Entry *e) {
	if (e->type() == KWallet::Wallet::Password) {
		return isPlainPassword(e) ? "password" : "binary";
	}
	return typeName(e->type());
}

}


KWalletSecretServiceStore::KWalletSecretServiceStore(const QString& name)
: _name(name), _open(false) {
}


KWalletSecretServiceStore::~KWalletSecretServiceStore() {
	clear();
}


void KWalletSecretServiceStore::clear() {
	for (FolderMap::Iterator f = _entries.begin(); f != _entries.end(); ++f) {
		for (EntryMap::Iterator e = f.data().begin(); e != f.data().end(); ++e) {
			delete e.data();  // ~Entry() wipes the value
		}
	}
	_entries.clear();
	_open = false;
}


bool KWalletSecretServiceStore::serviceAvailable() {
	GError *err = 0L;
	GDBusConnection *bus = g_bus_get_sync(G_BUS_TYPE_SESSION, 0L, &err);
	if (!bus) {
		if (err) {
			g_error_free(err);
		}
		return false;
	}

	bool rc = false;
	GVariant *r = g_dbus_connection_call_sync(bus, "org.freedesktop.DBus",
			"/org/freedesktop/DBus", "org.freedesktop.DBus", "NameHasOwner",
			g_variant_new("(s)", "org.freedesktop.secrets"), G_VARIANT_TYPE("(b)"),
			G_DBUS_CALL_FLAGS_NONE, 5000, 0L, 0L);
	if (r) {
		gboolean owned = FALSE;
		g_variant_get(r, "(b)", &owned);
		rc = owned;
		g_variant_unref(r);
	}

	if (!rc) {
		r = g_dbus_connection_call_sync(bus, "org.freedesktop.DBus",
				"/org/freedesktop/DBus", "org.freedesktop.DBus", "ListActivatableNames",
				0L, G_VARIANT_TYPE("(as)"),
				G_DBUS_CALL_FLAGS_NONE, 5000, 0L, 0L);
		if (r) {
			GVariantIter *it = 0L;
			const char *n;
			g_variant_get(r, "(as)", &it);
			while (!rc && g_variant_iter_loop(it, "&s", &n)) {
				rc = (strcmp(n, "org.freedesktop.secrets") == 0);
			}
			g_variant_iter_free(it);
			g_variant_unref(r);
		}
	}

	g_object_unref(bus);
	return rc;
}


int KWalletSecretServiceStore::open() {
	if (_open) {
		return -255;
	}

	SecretService *s = service();
	if (!s) {
		return -1;
	}

	// The default collection ("login" under GNOME); unlocked by
	// pam_gnome_keyring at login, else the service asks for its
	// password itself.
	GError *err = 0L;
	SecretCollection *c = secret_collection_for_alias_sync(s, SECRET_COLLECTION_DEFAULT,
			SECRET_COLLECTION_NONE, 0L, &err);
	if (!c && err) {
		warn("cannot look up the default collection", err);
		reset(s);
		return -1;
	}
	if (!c) {
		// none yet: the service creates it (and asks for a password)
		c = secret_collection_create_sync(s, "Login", SECRET_COLLECTION_DEFAULT,
				SECRET_COLLECTION_CREATE_NONE, 0L, &err);
		if (!c) {
			warn("cannot create the default collection", err);
			reset(s);
			return -1;
		}
	}
	if (secret_collection_get_locked(c)) {
		GList *l = g_list_append(0L, c);
		secret_service_unlock_sync(s, l, 0L, 0L, &err);
		g_list_free(l);
		if (err) {
			warn("cannot unlock the default collection", err);
			err = 0L;
		}
		if (secret_collection_get_locked(c)) {
			// the user cancelled the service's prompt
			g_object_unref(c);
			done(s);
			return -1;
		}
	}
	g_object_unref(c);

	GHashTable *a = attributes(WholeWallet, _name);
	GList *items = secret_service_search_sync(s, schema(), a,
			SecretSearchFlags(SECRET_SEARCH_ALL | SECRET_SEARCH_UNLOCK | SECRET_SEARCH_LOAD_SECRETS),
			0L, &err);
	g_hash_table_unref(a);
	if (err) {
		warn("cannot search the wallet", err);
		g_list_free_full(items, g_object_unref);
		reset(s);
		return -1;
	}

	clear();
	for (GList *i = items; i; i = i->next) {
		SecretItem *item = SECRET_ITEM(i->data);
		GHashTable *ia = secret_item_get_attributes(item);
		bool complete = g_hash_table_lookup(ia, "folder") && g_hash_table_lookup(ia, "key");
		QString folder = nn(attribute(ia, "folder"));
		QString key = nn(attribute(ia, "key"));
		QString type = attribute(ia, "type");
		g_hash_table_unref(ia);

		if (!complete) {
			continue;
		}
		if (!_entries.contains(folder)) {
			_entries.insert(folder, EntryMap());
		}
		if (type == "folder") {
			continue;
		}

		SecretValue *v = secret_item_get_secret(item);
		if (!v) {
			// still locked (another collection, prompt cancelled)
			kdWarning() << "kwalletd (Secret Service): skipping a locked item of wallet " << _name << endl;
			continue;
		}
		gsize len = 0;
		const gchar *data = secret_value_get(v, &len);

		KWallet::Entry *e = new KWallet::Entry;
		e->setKey(key);
		if (type == "password") {
			e->setType(KWallet::Wallet::Password);
			QString p = QString::fromUtf8(data, int(len));
			e->setValue(p);
			p.fill(QChar(0));
		} else {
			QByteArray b;
			b.duplicate(data, uint(len));
			e->setValue(b);
			b.fill(0);
			if (type == "binary") {
				e->setType(KWallet::Wallet::Password);
			} else if (type == "map") {
				e->setType(KWallet::Wallet::Map);
			} else if (type == "stream") {
				e->setType(KWallet::Wallet::Stream);
			} else {
				e->setType(KWallet::Wallet::Unknown);
			}
		}
		secret_value_unref(v);

		EntryMap& m = _entries[folder];
		if (m.contains(key)) {
			delete m[key];
		}
		m[key] = e;
	}
	g_list_free_full(items, g_object_unref);
	done(s);

	_open = true;
	return 0;
}


int KWalletSecretServiceStore::sync(const QByteArray&) {
	// every change is written through
	return 0;
}


int KWalletSecretServiceStore::close(const QByteArray&) {
	clear();
	return 0;
}


namespace {

// Stores (replaces) one item; after an error (the service restarted and
// its session is gone) once more with a new connection.
bool storeItem(GHashTable *a, const QString& label, SecretValue *v) {
	bool ok = false;
	for (int attempt = 0; !ok && attempt < 2; ++attempt) {
		SecretService *s = service();
		if (!s) {
			break;
		}
		GError *err = 0L;
		ok = secret_service_store_sync(s, schema(), a, SECRET_COLLECTION_DEFAULT,
				label.utf8().data(), v, 0L, &err);
		if (ok) {
			done(s);
		} else {
			warn("cannot store an item", err);
			reset(s);
		}
	}
	g_hash_table_unref(a);
	secret_value_unref(v);
	return ok;
}


// Removes the matching items, with one retry like storeItem().
bool clearItems(GHashTable *a) {
	bool ok = false;
	for (int attempt = 0; !ok && attempt < 2; ++attempt) {
		SecretService *s = service();
		if (!s) {
			break;
		}
		GError *err = 0L;
		secret_service_clear_sync(s, schema(), a, 0L, &err);
		if (err) {
			warn("cannot remove items", err);
			reset(s);
		} else {
			done(s);
			ok = true;
		}
	}
	g_hash_table_unref(a);
	return ok;
}

}


bool KWalletSecretServiceStore::store(const QString& folder, const KWallet::Entry *e) {
	const char *type = storedType(e);
	const char *ct;
	QByteArray secret;
	if (!strcmp(type, "password")) {
		ct = CT_TEXT;
		QString p = e->password();
		QCString u = p.utf8();
		secret.duplicate(u.data(), u.length());
		u.fill(0);
		p.fill(QChar(0));
	} else {
		ct = !strcmp(type, "binary") ? CT_ENTRY : !strcmp(type, "map") ? CT_MAP : CT_STREAM;
		secret.duplicate(e->value());
	}

	QString label = _name == "kdewallet"
		? QString("KWallet: %1/%2").arg(folder).arg(e->key())
		: QString("KWallet (%1): %2/%3").arg(_name).arg(folder).arg(e->key());

	SecretValue *v = secret_value_new(secret.size() ? secret.data() : "", secret.size(), ct);
	secret.fill(0);
	return storeItem(attributes(OneEntry, _name, folder, e->key(), type), label, v);
}


bool KWalletSecretServiceStore::erase(const QString& folder, const QString& key) {
	return clearItems(attributes(OneEntry, _name, folder, key));
}


bool KWalletSecretServiceStore::eraseFolder(const QString& folder) {
	return clearItems(attributes(WholeFolder, _name, folder));
}


int KWalletSecretServiceStore::reload() {
	clear();
	return open();
}


QStringList KWalletSecretServiceStore::folderList() const {
	return _entries.keys();
}


bool KWalletSecretServiceStore::hasFolder(const QString& f) const {
	return _entries.contains(nn(f));
}


bool KWalletSecretServiceStore::createFolder(const QString& folder) {
	const QString f = nn(folder);
	if (!_open || _entries.contains(f)) {
		return false;
	}
	// a folder marker: type "folder", key "", no secret
	QString label = _name == "kdewallet"
		? QString("KWallet: %1").arg(f)
		: QString("KWallet (%1): %2").arg(_name).arg(f);
	if (!storeItem(attributes(OneEntry, _name, f, "", "folder"), label, secret_value_new("", 0, CT_STREAM))) {
		return false;
	}
	_entries.insert(f, EntryMap());
	return true;
}


bool KWalletSecretServiceStore::removeFolder(const QString& folder) {
	const QString f = nn(folder);
	if (!_open) {
		return false;
	}
	FolderMap::Iterator fi = _entries.find(f);
	if (fi == _entries.end()) {
		return false;
	}
	if (!eraseFolder(f)) {
		return false;
	}
	if (_folder == f) {
		_folder = QString::null;
	}
	for (EntryMap::Iterator ei = fi.data().begin(); ei != fi.data().end(); ++ei) {
		delete ei.data();
	}
	_entries.remove(fi);
	return true;
}


QStringList KWalletSecretServiceStore::entryList() const {
	if (!_entries.contains(_folder)) {
		return QStringList();
	}
	return _entries[_folder].keys();
}


KWallet::Entry *KWalletSecretServiceStore::readEntry(const QString& k) {
	const QString key = nn(k);
	if (_open && hasEntry(key)) {
		return _entries[_folder][key];
	}
	return 0L;
}


QPtrList<KWallet::Entry> KWalletSecretServiceStore::readEntryList(const QString& key) {
	QPtrList<KWallet::Entry> rc;
	if (!_open || !_entries.contains(_folder)) {
		return rc;
	}
	QRegExp re(key, true, true);
	const EntryMap& map = _entries[_folder];
	for (EntryMap::ConstIterator i = map.begin(); i != map.end(); ++i) {
		if (re.exactMatch(i.key())) {
			rc.append(i.data());
		}
	}
	return rc;
}


bool KWalletSecretServiceStore::writeEntry(KWallet::Entry *entry) {
	if (!_open) {
		return false;
	}
	KWallet::Entry copy;
	KWallet::Entry *e = entry;
	if (entry->key().isNull()) {
		copy.copy(entry);
		copy.setKey(nn(entry->key()));
		e = &copy;
	}
	if (!_entries.contains(_folder) && !createFolder(_folder)) {
		return false;
	}
	EntryMap& m = _entries[_folder];
	if (m.contains(e->key()) && strcmp(storedType(m[e->key()]), storedType(e)) != 0) {
		// another type is another item: remove the old one first
		if (!erase(_folder, e->key())) {
			return false;
		}
		delete m[e->key()];
		m.remove(e->key());
	}
	if (!store(_folder, e)) {
		return false;
	}
	if (!m.contains(e->key())) {
		m[e->key()] = new KWallet::Entry;
	}
	m[e->key()]->copy(e);
	return true;
}


bool KWalletSecretServiceStore::hasEntry(const QString& k) const {
	const QString key = nn(k);
	return _entries.contains(_folder) && _entries[_folder].contains(key);
}


bool KWalletSecretServiceStore::removeEntry(const QString& k) {
	const QString key = nn(k);
	if (!_open || !hasEntry(key)) {
		return false;
	}
	if (!erase(_folder, key)) {
		return false;
	}
	EntryMap& m = _entries[_folder];
	delete m[key];
	m.remove(key);
	return true;
}


int KWalletSecretServiceStore::renameEntry(const QString& o, const QString& n) {
	const QString oldName = nn(o), newName = nn(n);
	if (!_open || !_entries.contains(_folder)) {
		return -1;
	}
	EntryMap& m = _entries[_folder];
	if (!m.contains(oldName) || m.contains(newName)) {
		return -1;
	}
	KWallet::Entry *e = m[oldName];
	e->setKey(newName);
	if (!store(_folder, e)) {
		e->setKey(oldName);
		return -1;
	}
	m.remove(oldName);
	m[newName] = e;
	if (!erase(_folder, oldName)) {
		return -1;  // both items exist now; the new name is in effect
	}
	return 0;
}


bool KWalletSecretServiceStore::folderDoesNotExist(const QString& folder) const {
	return !_entries.contains(nn(folder));
}


bool KWalletSecretServiceStore::entryDoesNotExist(const QString& folder, const QString& entry) const {
	return !_entries.contains(nn(folder)) || !_entries[nn(folder)].contains(nn(entry));
}


namespace {

// The attributes of the matching items, without unlocking or reading
// secrets.  Returns false on error.
bool searchAttributes(GHashTable *query, QValueList<QMap<QString,QString> >& rc) {
	SecretService *s = service();
	if (!s) {
		g_hash_table_unref(query);
		return false;
	}
	GError *err = 0L;
	GList *items = secret_service_search_sync(s, schema(), query, SECRET_SEARCH_ALL, 0L, &err);
	g_hash_table_unref(query);
	if (err) {
		warn("cannot search", err);
		g_list_free_full(items, g_object_unref);
		reset(s);
		return false;
	}
	for (GList *i = items; i; i = i->next) {
		GHashTable *ia = secret_item_get_attributes(SECRET_ITEM(i->data));
		QMap<QString,QString> m;
		GHashTableIter it;
		gpointer k, v;
		g_hash_table_iter_init(&it, ia);
		while (g_hash_table_iter_next(&it, &k, &v)) {
			m[QString::fromUtf8(static_cast<const char*>(k))] = QString::fromUtf8(static_cast<const char*>(v));
		}
		g_hash_table_unref(ia);
		rc.append(m);
	}
	g_list_free_full(items, g_object_unref);
	done(s);
	return true;
}

}


QStringList KWalletSecretServiceStore::walletNames() {
	QStringList rc;
	QValueList<QMap<QString,QString> > l;
	GHashTable *a = g_hash_table_new_full(g_str_hash, g_str_equal, 0L, g_free);
	if (searchAttributes(a, l)) {
		for (QValueList<QMap<QString,QString> >::ConstIterator i = l.begin(); i != l.end(); ++i) {
			QString w = (*i)["wallet"];
			if (!w.isEmpty() && !rc.contains(w)) {
				rc.append(w);
			}
		}
	}
	return rc;
}


bool KWalletSecretServiceStore::deleteWallet(const QString& wallet) {
	SecretService *s = service();
	if (!s) {
		return false;
	}
	GHashTable *a = attributes(WholeWallet, wallet);
	GError *err = 0L;
	gboolean rc = secret_service_clear_sync(s, schema(), a, 0L, &err);
	g_hash_table_unref(a);
	if (err) {
		warn("cannot delete the wallet", err);
		reset(s);
		return false;
	}
	done(s);
	return rc;
}


bool KWalletSecretServiceStore::folderExists(const QString& wallet, const QString& folder) {
	QValueList<QMap<QString,QString> > l;
	return searchAttributes(attributes(WholeFolder, wallet, folder), l) && !l.isEmpty();
}


bool KWalletSecretServiceStore::entryExists(const QString& wallet, const QString& folder, const QString& key) {
	QValueList<QMap<QString,QString> > l;
	return searchAttributes(attributes(OneEntry, wallet, folder, key), l) && !l.isEmpty();
}
