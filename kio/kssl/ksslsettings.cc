/* This file is part of the KDE project
 *
 * Copyright (C) 2000 George Staikos <staikos@kde.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
 
#include <stdlib.h>
#include <pwd.h>
#include <unistd.h>

#include <qfile.h>
#include <qsortedlist.h>

#include "ksslsettings.h"
#include <kglobal.h>
#include <kstandarddirs.h>
#include <kdebug.h>

#include "ksslopenssl_p.h"


      class CipherNode {
      public:
        CipherNode(const char *_name, int _keylen) : 
                      name(_name), keylen(_keylen) {}
        QString name;
        int keylen;
        inline int operator==(CipherNode &x) 
                     { return ((x.keylen == keylen) && (x.name == name)); }
        inline int operator< (CipherNode &x) { return keylen < x.keylen;  }
        inline int operator<=(CipherNode &x) { return keylen <= x.keylen; }
        inline int operator> (CipherNode &x) { return keylen > x.keylen;  }
        inline int operator>=(CipherNode &x) { return keylen >= x.keylen; }
      };


class KSSLSettingsPrivate {
public:
  KSSLSettingsPrivate() {
  }
  ~KSSLSettingsPrivate() {

  }

  bool m_bUseEGD;
  bool m_bUseEFile;
  QString m_EGDPath;
  bool m_bSendX509;
  bool m_bPromptX509;
};

//
// FIXME
// Implementation note: for now, we only read cipher settings from disk,
//                      and do not store them in memory.  This should change.
//

KSSLSettings::KSSLSettings(bool readConfig) {
  d = new KSSLSettingsPrivate;
  m_cfg = new KConfig("cryptodefaults", false, false);

  if (!KGlobal::dirs()->addResourceType("kssl", KStandardDirs::kde_default("data") + "kssl")) {
    //kdDebug(7029) << "Error adding (kssl, share/apps/kssl)" << endl;
  }

  if (readConfig) load();
}


// we don't save settings incase it was a temporary object
KSSLSettings::~KSSLSettings() {
  delete m_cfg;
  delete d;
}
 

bool KSSLSettings::sslv2() const {
  return m_bUseSSLv2;
}


bool KSSLSettings::sslv3() const {
  return m_bUseSSLv3;
}


bool KSSLSettings::tlsv1() const {
  return m_bUseTLSv1;
}

 
// An empty list means: leave the cipher list of the SSL context alone, so
// that the system's default (the OpenSSL configuration, a distribution's
// crypto policy) applies.  A list is returned only when the user disabled
// some of those ciphers in the crypto settings; it is then the system's
// list, in its order, without these.
QString KSSLSettings::getCipherList() {
	QString clist;
#ifdef KSSL_HAVE_SSL
	QString tcipher;
	bool firstcipher = true;
	bool restricted = false;
	QPtrList<CipherNode> cipherList;

	cipherList.setAutoDelete(true);

	// OpenSSL 3 has no SSLv2 and no SSLv3 method: the ciphers are
	// those of the TLS method (kcontrol's crypto module shows those),
	// whatever the SSLv2/SSLv3 settings.
	const SSL_METHOD *meth = TLS_client_method();

	SSL_CTX *ctx = SSL_CTX_new(meth);
	SSL* ssl = SSL_new(ctx);
	STACK_OF(SSL_CIPHER)* sk = SSL_get_ciphers(ssl);
	int cnt = sk_SSL_CIPHER_num(sk);
	for (int i=0; i< cnt; i++) {
		const SSL_CIPHER *sc = sk_SSL_CIPHER_value(sk,i);
		if (!sc)
			break;

		// The TLS 1.3 suites are not set by SSL_CTX_set_cipher_list()
		// and cannot be turned off here; kcontrol does not show them.
		if (!strcmp("TLSv1.3", SSL_CIPHER_get_version(sc)))
			continue;

		if(!strcmp("SSLv2", SSL_CIPHER_get_version(sc)))
			m_cfg->setGroup("SSLv2");
		else
			m_cfg->setGroup("SSLv3");

		tcipher.sprintf("cipher_%s", SSL_CIPHER_get_name(sc));
		int bits = SSL_CIPHER_get_bits(sc, NULL);
		if (m_cfg->readBoolEntry(tcipher, bits >= 56)) {
			CipherNode *xx = new CipherNode(SSL_CIPHER_get_name(sc),bits);
			if (!cipherList.contains(xx))
				cipherList.prepend(xx);
			else
				delete xx;
		} else if (m_cfg->hasKey(tcipher))
			restricted = true;	// disabled by the user
	}
	SSL_free(ssl);
	SSL_CTX_free(ctx);

	if (!restricted) {
		kdDebug(7029) << "Cipher list: the system default" << endl;
		return QString::null;
	}

	// Remove any ADH ciphers as per RFC2246
	// Also remove NULL ciphers and 168bit ciphers
	for (unsigned int i = 0; i < cipherList.count(); i++) {
		CipherNode *j = 0L;
		while ((j = cipherList.at(i)) != 0L) {
			if (j->name.contains("ADH-") || j->name.contains("NULL-") || j->name.contains("DES-CBC3-SHA") || j->name.contains("FZA")) {
				cipherList.remove(j);
			} else {
				break;
			}
		}
	} 

	// now assemble the list  cipher1:cipher2:cipher3:...:ciphern
	while (!cipherList.isEmpty()) {
		if (firstcipher)
			firstcipher = false;
		else clist.append(":");
		clist.append(cipherList.getLast()->name);
		cipherList.removeLast();
	} // while

	kdDebug(7029) << "Cipher list is: " << clist << endl;

#endif
	return clist;
}

// FIXME - sync these up so that we can use them with the control module!!
void KSSLSettings::load() {
  m_cfg->reparseConfiguration();

  m_cfg->setGroup("TLS");
  m_bUseTLSv1 = m_cfg->readBoolEntry("Enabled", true);
 
  m_cfg->setGroup("SSLv2");
  m_bUseSSLv2 = m_cfg->readBoolEntry("Enabled", false);
 
  m_cfg->setGroup("SSLv3");
  m_bUseSSLv3 = m_cfg->readBoolEntry("Enabled", true);
 
  m_cfg->setGroup("Warnings");
  m_bWarnOnEnter = m_cfg->readBoolEntry("OnEnter", false);
  m_bWarnOnLeave = m_cfg->readBoolEntry("OnLeave", true);
  m_bWarnOnUnencrypted = m_cfg->readBoolEntry("OnUnencrypted", true);
  m_bWarnOnMixed = m_cfg->readBoolEntry("OnMixed", true);

  m_cfg->setGroup("Validation");
  m_bWarnSelfSigned = m_cfg->readBoolEntry("WarnSelfSigned", true);
  m_bWarnExpired = m_cfg->readBoolEntry("WarnExpired", true);
  m_bWarnRevoked = m_cfg->readBoolEntry("WarnRevoked", true);

  m_cfg->setGroup("EGD");
  d->m_bUseEGD = m_cfg->readBoolEntry("UseEGD", false);
  d->m_bUseEFile = m_cfg->readBoolEntry("UseEFile", false);
  d->m_EGDPath = m_cfg->readPathEntry("EGDPath");

  m_cfg->setGroup("Auth");
  d->m_bSendX509 = ("send" == m_cfg->readEntry("AuthMethod", ""));
  d->m_bPromptX509 = ("prompt" == m_cfg->readEntry("AuthMethod", ""));

  #ifdef KSSL_HAVE_SSL



  #endif
}


void KSSLSettings::defaults() {
  m_bUseTLSv1 = true;
  m_bUseSSLv2 = false;
  m_bUseSSLv3 = true;
  m_bWarnOnEnter = false;
  m_bWarnOnLeave = true;
  m_bWarnOnUnencrypted = true;
  m_bWarnOnMixed = true;
  m_bWarnSelfSigned = true;
  m_bWarnExpired = true;
  m_bWarnRevoked = true;
  d->m_bUseEGD = false;
  d->m_bUseEFile = false;
  d->m_EGDPath = "";
}


void KSSLSettings::save() {
  m_cfg->setGroup("TLS");
  m_cfg->writeEntry("Enabled", m_bUseTLSv1);
 
  m_cfg->setGroup("SSLv2");
  m_cfg->writeEntry("Enabled", m_bUseSSLv2);
 
  m_cfg->setGroup("SSLv3");
  m_cfg->writeEntry("Enabled", m_bUseSSLv3);
 
  m_cfg->setGroup("Warnings");
  m_cfg->writeEntry("OnEnter", m_bWarnOnEnter);
  m_cfg->writeEntry("OnLeave", m_bWarnOnLeave);
  m_cfg->writeEntry("OnUnencrypted", m_bWarnOnUnencrypted);
  m_cfg->writeEntry("OnMixed", m_bWarnOnMixed);

  m_cfg->setGroup("Validation");
  m_cfg->writeEntry("WarnSelfSigned", m_bWarnSelfSigned);
  m_cfg->writeEntry("WarnExpired", m_bWarnExpired);
  m_cfg->writeEntry("WarnRevoked", m_bWarnRevoked);

  m_cfg->setGroup("EGD");
  m_cfg->writeEntry("UseEGD", d->m_bUseEGD);
  m_cfg->writeEntry("UseEFile", d->m_bUseEFile);
  m_cfg->writePathEntry("EGDPath", d->m_EGDPath);

  m_cfg->sync();
  // FIXME - ciphers
#if 0
#ifdef KSSL_HAVE_SSL
  m_cfg->setGroup("SSLv2");
  for (unsigned int i = 0; i < v2ciphers.count(); i++) {
    QString ciphername;
    ciphername.sprintf("cipher_%s", v2ciphers[i].ascii());
    if (v2selectedciphers.contains(v2ciphers[i])) {
      m_cfg->writeEntry(ciphername, true);
    } else m_cfg->writeEntry(ciphername, false);
  }
 
  m_cfg->setGroup("SSLv3");
  for (unsigned int i = 0; i < v3ciphers.count(); i++) {
    QString ciphername;
    ciphername.sprintf("cipher_%s", v3ciphers[i].ascii());
    if (v3selectedciphers.contains(v3ciphers[i])) {
      m_cfg->writeEntry(ciphername, true);
    } else m_cfg->writeEntry(ciphername, false);
  }
#endif 
 
  m_cfg->sync();
 
  // insure proper permissions -- contains sensitive data
  QString cfgName(KGlobal::dirs()->findResource("config", "cryptodefaults"));
  if (!cfgName.isEmpty())
    ::chmod(QFile::encodeName(cfgName), 0600);
#endif
}


bool KSSLSettings::warnOnEnter() const       { return m_bWarnOnEnter; }
void KSSLSettings::setWarnOnEnter(bool x)    { m_bWarnOnEnter = x; }
bool KSSLSettings::warnOnUnencrypted() const { return m_bWarnOnUnencrypted; }
void KSSLSettings::setWarnOnUnencrypted(bool x) { m_bWarnOnUnencrypted = x; }
bool KSSLSettings::warnOnLeave() const       { return m_bWarnOnLeave; }
void KSSLSettings::setWarnOnLeave(bool x)    { m_bWarnOnLeave = x; }
bool KSSLSettings::warnOnMixed() const       { return m_bWarnOnMixed; }
bool KSSLSettings::warnOnSelfSigned() const  { return m_bWarnSelfSigned; }
bool KSSLSettings::warnOnRevoked() const     { return m_bWarnRevoked; }
bool KSSLSettings::warnOnExpired() const     { return m_bWarnExpired; }
bool KSSLSettings::useEGD() const            { return d->m_bUseEGD;      }
bool KSSLSettings::useEFile() const          { return d->m_bUseEFile;    }
bool KSSLSettings::autoSendX509() const      { return d->m_bSendX509; }
bool KSSLSettings::promptSendX509() const    { return d->m_bPromptX509; }

void KSSLSettings::setTLSv1(bool enabled) { m_bUseTLSv1 = enabled; }
void KSSLSettings::setSSLv2(bool enabled) { m_bUseSSLv2 = enabled; }
void KSSLSettings::setSSLv3(bool enabled) { m_bUseSSLv3 = enabled; }

QString& KSSLSettings::getEGDPath()       { return d->m_EGDPath; }


