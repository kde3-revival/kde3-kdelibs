// KWallet::Wallet through DCOP and kded's kwalletd with the Secret Service
// backend, against a private keyring: run through run-tests.sh.
//   kwtest api       every Wallet call
//   kwtest reread    what "api" left, from another process
//   kwtest kmail     KMail: network wallet, folder "kmail", account password
//   kwtest khtml     KHTML forms: keyDoesNotExist, asynchronous open, maps
//   kwtest migrated  the entries of the imported classic wallet
//   kwtest classic   Backend=File: the classic wallet file
//   kwtest fails     opening fails (service gone or locked), no dialog
#include <kwallet.h>

#include <kapplication.h>
#include <kcmdlineargs.h>
#include <kaboutdata.h>
#include <qtimer.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

using KWallet::Wallet;

static int failures = 0;
#define CHECK(c) do { if (c) printf("PASS %s\n", #c); else { printf("FAIL %s (line %d)\n", #c, __LINE__); ++failures; } } while (0)

static QMap<QString,QString> aMap() {
	QMap<QString,QString> m;
	m["login"] = QString::fromUtf8("ș€rghei");
	m["pass"] = "p4ss";
	return m;
}

static bool same(const QMap<QString,QString>& a, const QMap<QString,QString>& b) {
	if (a.count() != b.count()) {
		return false;
	}
	for (QMap<QString,QString>::ConstIterator i = a.begin(); i != a.end(); ++i) {
		if (!b.contains(i.key()) || b[i.key()] != i.data()) {
			return false;
		}
	}
	return true;
}

static QByteArray bytes() {
	QByteArray b(4);
	b[0] = 0; b[1] = char(0xfe); b[2] = 'x'; b[3] = 0;
	return b;
}

static void api() {
	CHECK(Wallet::isEnabled());
	CHECK(Wallet::walletList().contains(Wallet::LocalWallet()));
	CHECK(!Wallet::isOpen(Wallet::LocalWallet()));
	Wallet *w = Wallet::openWallet(Wallet::LocalWallet(), 0, Wallet::Synchronous);
	CHECK(w && w->isOpen());
	if (!w) {
		return;
	}
	CHECK(Wallet::isOpen(Wallet::LocalWallet()));
	CHECK(w->walletName() == Wallet::LocalWallet());
	// a new wallet has the two standard folders
	CHECK(w->hasFolder(Wallet::PasswordFolder()));
	CHECK(w->hasFolder(Wallet::FormDataFolder()));
	CHECK(w->createFolder("Test"));
	CHECK(w->hasFolder("Test"));
	CHECK(w->setFolder("Test"));
	CHECK(w->currentFolder() == "Test");
	CHECK(w->writePassword("pw", QString::fromUtf8("pässwörd")) == 0);
	CHECK(w->writeMap("map", aMap()) == 0);
	CHECK(w->writeEntry("bin", bytes()) == 0);
	CHECK(w->writeEntry("typed", bytes(), Wallet::Stream) == 0);
	CHECK(w->writePassword("pw2", "second") == 0);
	QString p;
	CHECK(w->readPassword("pw", p) == 0 && p == QString::fromUtf8("pässwörd"));
	QMap<QString,QString> m;
	CHECK(w->readMap("map", m) == 0 && same(m, aMap()));
	QByteArray b;
	CHECK(w->readEntry("bin", b) == 0 && b == bytes());
	CHECK(w->entryType("pw") == Wallet::Password);
	CHECK(w->entryType("map") == Wallet::Map);
	CHECK(w->entryType("bin") == Wallet::Stream);
	CHECK(w->entryType("nope") == Wallet::Unknown);
	CHECK(w->hasEntry("pw") && !w->hasEntry("nope"));
	QMap<QString,QString> pl;
	CHECK(w->readPasswordList("pw*", pl) == 0 && pl.count() == 2 && pl["pw2"] == "second");
	QMap<QString, QMap<QString,QString> > ml;
	CHECK(w->readMapList("*", ml) == 0 && ml.count() == 1 && same(ml["map"], aMap()));
	QMap<QString,QByteArray> el;
	CHECK(w->readEntryList("*", el) == 0 && el.count() == 5);
	CHECK(w->renameEntry("pw2", "pw3") == 0);
	CHECK(w->hasEntry("pw3") && !w->hasEntry("pw2"));
	CHECK(w->writeEntry("temp", bytes()) == 0);
	CHECK(w->removeEntry("temp") == 0 && !w->hasEntry("temp"));
	CHECK(w->entryList().count() == 5);
	CHECK(w->createFolder("Scratch") && w->setFolder("Scratch") && w->writePassword("x", "y") == 0);
	CHECK(w->removeFolder("Scratch") && !w->hasFolder("Scratch"));
	CHECK(w->folderList().contains("Test") && !w->folderList().contains("Scratch"));
	CHECK(w->sync() == 0);
	CHECK(!Wallet::folderDoesNotExist(Wallet::LocalWallet(), "Test"));
	CHECK(Wallet::folderDoesNotExist(Wallet::LocalWallet(), "Scratch"));
	CHECK(!Wallet::keyDoesNotExist(Wallet::LocalWallet(), "Test", "pw"));
	CHECK(Wallet::keyDoesNotExist(Wallet::LocalWallet(), "Test", "pw2"));
	CHECK(Wallet::users(Wallet::LocalWallet()).count() == 1);

	// a second wallet, deleted again
	Wallet *o = Wallet::openWallet("scratchwallet", 0, Wallet::Synchronous);
	CHECK(o && o->isOpen());
	if (o) {
		CHECK(o->createFolder("f") && o->setFolder("f") && o->writePassword("k", "v") == 0);
		CHECK(Wallet::walletList().contains("scratchwallet"));
		delete o;
	}
	CHECK(Wallet::deleteWallet("scratchwallet") == 0);
	CHECK(!Wallet::walletList().contains("scratchwallet"));

	CHECK(Wallet::closeWallet(Wallet::LocalWallet(), true) == 0);
	CHECK(!Wallet::isOpen(Wallet::LocalWallet()));
	delete w;
}

static void reread() {
	// still closed: answered from the attributes
	CHECK(!Wallet::keyDoesNotExist(Wallet::LocalWallet(), "Test", "pw"));
	Wallet *w = Wallet::openWallet(Wallet::LocalWallet(), 0, Wallet::Synchronous);
	CHECK(w && w->isOpen());
	if (!w) {
		return;
	}
	CHECK(w->setFolder("Test"));
	QString p;
	CHECK(w->readPassword("pw", p) == 0 && p == QString::fromUtf8("pässwörd"));
	CHECK(w->readPassword("pw3", p) == 0 && p == "second");
	QMap<QString,QString> m;
	CHECK(w->readMap("map", m) == 0 && same(m, aMap()));
	QByteArray b;
	CHECK(w->readEntry("typed", b) == 0 && b == bytes());
	delete w;
}

static void kmail() {
	Wallet *w = Wallet::openWallet(Wallet::NetworkWallet(), 0);
	CHECK(w && w->isOpen());
	if (!w) {
		return;
	}
	if (!w->hasFolder("kmail")) {
		w->createFolder("kmail");
	}
	CHECK(w->setFolder("kmail"));
	QString p;
	if (w->readPassword("account-1", p) == 0 && !p.isEmpty()) {
		CHECK(p == "imap-secret");  // the second run reads what the first wrote
	} else {
		CHECK(w->writePassword("account-1", "imap-secret") == 0);
	}
	delete w;
}


static void khtml(KApplication& app) {
	QString key = "http://www.example.org/login.html#loginform";
	bool fresh = Wallet::keyDoesNotExist(Wallet::NetworkWallet(), Wallet::FormDataFolder(), key);
	Wallet *w = Wallet::openWallet(Wallet::NetworkWallet(), 0, Wallet::Asynchronous);
	CHECK(w != 0);
	if (!w) {
		return;
	}
	for (int i = 0; i < 100 && !w->isOpen(); ++i) {
		app.processEvents();
		usleep(100000);
	}
	CHECK(w->isOpen());
	if (!w->hasFolder(Wallet::FormDataFolder())) {
		w->createFolder(Wallet::FormDataFolder());
	}
	CHECK(w->setFolder(Wallet::FormDataFolder()));
	if (fresh) {
		CHECK(w->writeMap(key, aMap()) == 0);
	} else {
		QMap<QString,QString> m;
		CHECK(w->readMap(key, m) == 0 && same(m, aMap()));
	}
	delete w;
}

static void migrated() {
	Wallet *w = Wallet::openWallet(Wallet::LocalWallet(), 0, Wallet::Synchronous);
	CHECK(w && w->isOpen());
	if (!w) {
		return;
	}
	CHECK(w->setFolder("Old"));
	QString p;
	CHECK(w->readPassword("oldpw", p) == 0 && p == "from the kwl");
	QMap<QString,QString> m;
	CHECK(w->readMap("oldmap", m) == 0 && same(m, aMap()));
	CHECK(w->hasFolder("OldEmpty"));
	// an entry that existed in both: the Secret Service one was kept
	CHECK(w->setFolder("Test"));
	CHECK(w->readPassword("pw", p) == 0 && p == QString::fromUtf8("pässwörd"));
	delete w;
}

// Backend=File: the classic wallet file still works (password dialog)
static void classic() {
	Wallet *w = Wallet::openWallet(Wallet::LocalWallet(), 0, Wallet::Synchronous);
	CHECK(w && w->isOpen());
	if (!w) {
		return;
	}
	CHECK(w->setFolder("Old"));
	QString p;
	CHECK(w->readPassword("oldpw", p) == 0 && p == "from the kwl");
	CHECK(w->writePassword("added", "in the file") == 0);
	CHECK(w->readPassword("added", p) == 0 && p == "in the file");
	delete w;
}

static void fails() {
	Wallet *w = Wallet::openWallet(Wallet::LocalWallet(), 0, Wallet::Synchronous);
	CHECK(w == 0);
	delete w;
}

int main(int argc, char **argv) {
	if (argc < 2) {
		return 2;
	}
	QCString mode = argv[1];
	KAboutData about("kwtest", "kwtest", "1");
	int one = 1;
	KCmdLineArgs::init(one, argv, &about);
	KApplication app;
	if (mode == "api") api();
	else if (mode == "reread") reread();
	else if (mode == "kmail") kmail();
	else if (mode == "khtml") khtml(app);
	else if (mode == "migrated") migrated();
	else if (mode == "classic") classic();
	else if (mode == "fails") fails();
	else return 2;
	printf("%s: %d failures\n", mode.data(), failures);
	return failures ? 1 : 0;
}
