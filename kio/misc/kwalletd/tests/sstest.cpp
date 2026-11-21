// The Secret Service store of kwalletd against a private keyring: run
// through run-tests.sh, never against the user's session bus.
//   sstest store      every operation, written and read back
//   sstest check      what "store" left, read with a new store
//   sstest down       the service has gone: errors, no crash
//   sstest locked     the default collection is locked, the prompt fails
#include <libsecret/secret.h>

#include "kwalletsecretservice.h"

#include <kinstance.h>
#include <qdatastream.h>
#include <stdio.h>
#include <string.h>

static int failures = 0;
#define CHECK(c) do { if (c) printf("PASS %s\n", #c); else { printf("FAIL %s (line %d)\n", #c, __LINE__); ++failures; } } while (0)

using KWallet::Entry;
using KWallet::Wallet;

static QByteArray mapBytes() {
	QMap<QString,QString> m;
	m["user"] = QString::fromUtf8("ș€rghei");
	m["host"] = "imap.example.org";
	QByteArray b;
	QDataStream ds(b, IO_WriteOnly);
	ds << m;
	return b;
}

static QByteArray streamBytes() {
	QByteArray b(5);
	b[0] = 'a'; b[1] = 0; b[2] = char(0xff); b[3] = '\n'; b[4] = 'z';
	return b;
}

static bool write(KWalletStore& s, const QString& f, const QString& k, Wallet::EntryType t, const QByteArray& v) {
	s.setFolder(f);
	Entry e;
	e.setKey(k);
	e.setValue(v);
	e.setType(t);
	return s.writeEntry(&e);
}

static bool writePassword(KWalletStore& s, const QString& f, const QString& k, const QString& p) {
	s.setFolder(f);
	Entry e;
	e.setKey(k);
	e.setValue(p);
	e.setType(Wallet::Password);
	return s.writeEntry(&e);
}

static QString password(KWalletStore& s, const QString& f, const QString& k) {
	s.setFolder(f);
	Entry *e = s.readEntry(k);
	return (e && e->type() == Wallet::Password) ? e->password() : QString("<none>");
}

static int store() {
	CHECK(KWalletSecretServiceStore::serviceAvailable());
	KWalletSecretServiceStore s("kdewallet");
	CHECK(s.open() == 0);
	CHECK(s.isOpen());
	CHECK(s.isSecretService());
	CHECK(s.folderList().isEmpty());

	CHECK(s.createFolder("Passwords"));
	CHECK(!s.createFolder("Passwords"));
	CHECK(s.createFolder("Empty"));
	CHECK(writePassword(s, "Passwords", "plain", QString::fromUtf8("sécret ü 🔑")));
	CHECK(writePassword(s, "Passwords", "empty", QString("")));
	CHECK(write(s, "Form Data", "map", Wallet::Map, mapBytes()));
	CHECK(write(s, "Passwords", "stream", Wallet::Stream, streamBytes()));
	// a password entry with bytes that are not a QString: kept as they are
	CHECK(write(s, "Passwords", "raw", Wallet::Password, streamBytes()));
	CHECK(s.hasFolder("Form Data"));  // created implicitly

	// overwrite, rename, remove, type change
	CHECK(writePassword(s, "Passwords", "plain2", "first"));
	CHECK(writePassword(s, "Passwords", "plain2", "second"));
	CHECK(writePassword(s, "Passwords", "old", "renamed"));
	CHECK(s.renameEntry("old", "new") == 0);
	CHECK(s.renameEntry("missing", "x") == -1);
	CHECK(s.renameEntry("new", "plain") == -1);  // target exists
	CHECK(writePassword(s, "Passwords", "gone", "x"));
	CHECK(s.removeEntry("gone"));
	CHECK(!s.removeEntry("gone"));
	CHECK(writePassword(s, "Passwords", "retyped", "was a password"));
	CHECK(write(s, "Passwords", "retyped", Wallet::Stream, streamBytes()));

	// a folder with entries, removed
	CHECK(writePassword(s, "Doomed", "a", "1"));
	CHECK(writePassword(s, "Doomed", "b", "2"));
	CHECK(s.removeFolder("Doomed"));
	CHECK(!s.hasFolder("Doomed"));

	// wildcards
	s.setFolder("Passwords");
	CHECK(s.readEntryList("plain*").count() == 2);

	// another wallet
	KWalletSecretServiceStore o("other");
	CHECK(o.open() == 0);
	CHECK(writePassword(o, "Passwords", "plain", "other wallet"));
	CHECK(o.close(QByteArray()) == 0);
	CHECK(!o.isOpen());
	return failures;
}

static int check() {
	KWalletSecretServiceStore s("kdewallet");
	CHECK(s.open() == 0);
	QStringList f = s.folderList();
	CHECK(f.count() == 3 && f.contains("Passwords") && f.contains("Form Data") && f.contains("Empty"));
	CHECK(password(s, "Passwords", "plain") == QString::fromUtf8("sécret ü 🔑"));
	CHECK(password(s, "Passwords", "empty") == "");
	CHECK(password(s, "Passwords", "plain2") == "second");
	CHECK(password(s, "Passwords", "new") == "renamed");
	CHECK(!s.hasEntry("old"));
	CHECK(!s.hasEntry("gone"));
	s.setFolder("Passwords");
	CHECK(s.readEntry("stream") && s.readEntry("stream")->type() == Wallet::Stream && s.readEntry("stream")->value() == streamBytes());
	CHECK(s.readEntry("raw") && s.readEntry("raw")->type() == Wallet::Password && s.readEntry("raw")->value() == streamBytes());
	CHECK(s.readEntry("retyped") && s.readEntry("retyped")->type() == Wallet::Stream);
	// the foreign item (another schema, same attribute names) is not seen
	CHECK(!s.hasEntry("foreign"));
	s.setFolder("Form Data");
	CHECK(s.readEntry("map") && s.readEntry("map")->type() == Wallet::Map && s.readEntry("map")->map() == mapBytes());
	s.setFolder("Empty");
	CHECK(s.entryList().isEmpty());
	CHECK(!s.folderDoesNotExist("Empty"));
	CHECK(s.folderDoesNotExist("Doomed"));
	CHECK(!s.entryDoesNotExist("Passwords", "plain"));
	CHECK(s.entryDoesNotExist("Passwords", "gone"));

	CHECK(KWalletSecretServiceStore::folderExists("kdewallet", "Empty"));
	CHECK(!KWalletSecretServiceStore::folderExists("kdewallet", "Doomed"));
	CHECK(KWalletSecretServiceStore::entryExists("kdewallet", "Passwords", "plain"));
	CHECK(!KWalletSecretServiceStore::entryExists("kdewallet", "Passwords", "gone"));
	QStringList w = KWalletSecretServiceStore::walletNames();
	CHECK(w.count() == 2 && w.contains("kdewallet") && w.contains("other"));
	CHECK(KWalletSecretServiceStore::deleteWallet("other"));
	CHECK(!KWalletSecretServiceStore::deleteWallet("other"));
	CHECK(KWalletSecretServiceStore::walletNames() == QStringList("kdewallet"));
	return failures;
}

static int down() {
	CHECK(!KWalletSecretServiceStore::serviceAvailable());
	KWalletSecretServiceStore s("kdewallet");
	CHECK(s.open() < 0);
	CHECK(!s.isOpen());
	CHECK(!writePassword(s, "Passwords", "x", "y"));
	CHECK(KWalletSecretServiceStore::walletNames().isEmpty());
	CHECK(!KWalletSecretServiceStore::folderExists("kdewallet", "Passwords"));
	return failures;
}

static int locked() {
	KWalletSecretServiceStore s("kdewallet");
	CHECK(s.open() < 0);  // the unlock prompt could not be shown
	CHECK(!s.isOpen());
	return failures;
}

// a store whose service disappears while it is open
static int vanish() {
	KWalletSecretServiceStore s("kdewallet");
	CHECK(s.open() == 0);
	printf("READY\n");
	fflush(stdout);
	char buf[8];
	if (!fgets(buf, sizeof buf, stdin)) {
		return 1;
	}
	CHECK(!writePassword(s, "Passwords", "after", "y"));
	CHECK(!s.removeEntry("plain"));
	CHECK(password(s, "Passwords", "plain") == QString::fromUtf8("sécret ü 🔑"));  // cached
	return failures;
}

int main(int argc, char **argv) {
	KInstance inst("sstest");
	if (argc < 2) {
		return 2;
	}
	int rc = 2;
	if (!strcmp(argv[1], "store")) rc = store();
	else if (!strcmp(argv[1], "check")) rc = check();
	else if (!strcmp(argv[1], "down")) rc = down();
	else if (!strcmp(argv[1], "locked")) rc = locked();
	else if (!strcmp(argv[1], "vanish")) rc = vanish();
	printf("%s: %d failures\n", argv[1], failures);
	return rc;
}
