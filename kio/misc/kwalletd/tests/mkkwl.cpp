// Writes a classic wallet file for the migration test:
//   mkkwl <file.kwl> <password>
#include "kwalletbackend.h"

#include <kinstance.h>
#include <qdatastream.h>

int main(int argc, char **argv) {
	if (argc < 3) {
		return 2;
	}
	KInstance inst("mkkwl");
	KWallet::Backend b(QString::fromLocal8Bit(argv[1]), true);
	QByteArray pw;
	pw.duplicate(argv[2], strlen(argv[2]));
	if (b.open(pw) < 0) {
		return 1;
	}
	b.createFolder("Old");
	b.createFolder("OldEmpty");
	b.createFolder("Test");
	b.setFolder("Old");
	KWallet::Entry e;
	e.setKey("oldpw");
	e.setValue(QString("from the kwl"));
	e.setType(KWallet::Wallet::Password);
	b.writeEntry(&e);
	QMap<QString,QString> m;
	m["login"] = QString::fromUtf8("ș€rghei");
	m["pass"] = "p4ss";
	QByteArray mb;
	QDataStream ds(mb, IO_WriteOnly);
	ds << m;
	e.setKey("oldmap");
	e.setValue(mb);
	e.setType(KWallet::Wallet::Map);
	b.writeEntry(&e);
	b.setFolder("Test");
	e.setKey("pw");
	e.setValue(QString("older value, must not win"));
	e.setType(KWallet::Wallet::Password);
	b.writeEntry(&e);
	return b.close(pw) < 0 ? 1 : 0;
}
