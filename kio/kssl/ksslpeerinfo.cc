/* This file is part of the KDE project
 *
 * Copyright (C) 2000-2003 George Staikos <staikos@kde.org>
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

#include <qregexp.h>

#include "ksslpeerinfo.h"
#include <kdebug.h>

#include <ksockaddr.h>
#include <kextsock.h>
#include <netsupp.h>
#ifndef Q_WS_WIN //TODO kresolver not ported
#include "kresolver.h"
#endif

#include <kopenssl.h>

#ifdef KSSL_HAVE_SSL
#include <string.h>
#include <arpa/inet.h>
#endif

class KSSLPeerInfoPrivate {
public:
	KSSLPeerInfoPrivate() {}
	~KSSLPeerInfoPrivate() { }
	QString peerHost;
};



KSSLPeerInfo::KSSLPeerInfo() {
	d = new KSSLPeerInfoPrivate;
}

KSSLPeerInfo::~KSSLPeerInfo() {
	delete d;
}

KSSLCertificate& KSSLPeerInfo::getPeerCertificate() {
	return m_cert;
}

void KSSLPeerInfo::setPeerHost(QString realHost) {
	d->peerHost = realHost.stripWhiteSpace();
	while(d->peerHost.endsWith("."))
		d->peerHost.truncate(d->peerHost.length()-1);

#ifdef Q_WS_WIN //TODO kresolver not ported
	d->peerHost = d->peerHost.lower();
#else	
	d->peerHost = QString::fromLatin1(KNetwork::KResolver::domainToAscii(d->peerHost));
#endif	
}

#ifdef KSSL_HAVE_SSL
// The binary form of host if it is an IP address ("192.0.2.1", "::1",
// "[::1]", "fe80::1%eth0"): returns its length (4 or 16), or 0.
static int ipAddress(const QString &host, unsigned char *addr)
{
	QString h = host;
	if (h.startsWith("[") && h.endsWith("]"))
		h = h.mid(1, h.length() - 2);
	int zone = h.find('%');
	if (zone >= 0)
		h.truncate(zone);
	// latin1() of an empty QString may be a null pointer
	QCString a = h.latin1();
	if (a.isEmpty())
		return 0;
	if (inet_pton(AF_INET, a.data(), addr) == 1)
		return 4;
	if (inet_pton(AF_INET6, a.data(), addr) == 1)
		return 16;
	return 0;
}

// The text of an X509_NAME entry or a dNSName, or null if it is not
// plain ASCII: an embedded NUL ("www.bank.example\0.evil.example",
// CVE-2009-2702) or any character above 0x7F.  Host names are compared
// in ASCII only (IDNs as A-labels, "xn--..."); QString::lower() on
// other characters would fold some into ASCII (KELVIN SIGN U+212A to
// "k", U+0130 to "i").
static QString nameText(KOpenSSLProxy *kossl, ASN1_STRING *s)
{
	unsigned char *utf8 = 0;
	int len = s ? kossl->ASN1_STRING_to_UTF8(&utf8, s) : -1;
	if (len < 0 || !utf8)
		return QString::null;
	bool ascii = true;
	for (int i = 0; i < len; i++) {
		if (utf8[i] == 0 || utf8[i] > 0x7f) {
			ascii = false;
			break;
		}
	}
	QString rc;
	if (ascii)
		rc = QString::fromLatin1(reinterpret_cast<char *>(utf8), len);
	kossl->CRYPTO_free(utf8, __FILE__, __LINE__);
	return rc;
}
#endif

// The names the certificate is valid for, as RFC 6125 has them: the
// dNSNames and iPAddresses of the subjectAltName extension, and the
// common names (CN) of the subject only if there is no dNSName.  The
// common names are taken from the subject entries themselves, not by
// parsing the X509_NAME_oneline() text, in which another entry can
// fake one ("O=x/CN=localhost").
bool KSSLPeerInfo::certMatchesAddress() {
#ifdef KSSL_HAVE_SSL
	KOpenSSLProxy *kossl = KOpenSSLProxy::self();
	X509 *x = m_cert.getCert();
	if (!x)
		return false;

	unsigned char addr[16];
	int addrLen = ipAddress(d->peerHost, addr);

	QStringList cns;
	bool haveDNSName = false;
	GENERAL_NAMES *names = (GENERAL_NAMES *)kossl->X509_get_ext_d2i(x, NID_subject_alt_name, 0, 0);
	if (names) {
		int cnt = kossl->OPENSSL_sk_num((OPENSSL_STACK *)names);
		bool ipMatch = false;
		for (int i = 0; i < cnt; i++) {
			const GENERAL_NAME *val = (const GENERAL_NAME *)kossl->OPENSSL_sk_value((OPENSSL_STACK *)names, i);
			if (val->type == GEN_DNS) {
				// even one that is skipped below
				haveDNSName = true;
				QString s = nameText(kossl, val->d.ia5);
				if (!s.isEmpty())
					cns += s;
			} else if (val->type == GEN_IPADD && addrLen > 0 &&
				   kossl->ASN1_STRING_length(val->d.ip) == addrLen &&
				   memcmp(kossl->ASN1_STRING_data(val->d.ip), addr, addrLen) == 0) {
				ipMatch = true;
			}
		}
		kossl->GENERAL_NAMES_free(names);
		if (ipMatch)
			return true;
	}

	if (!haveDNSName) {
		X509_NAME *subject = kossl->X509_get_subject_name(x);
		for (int i = -1; subject && (i = kossl->X509_NAME_get_index_by_NID(subject, NID_commonName, i)) >= 0; ) {
			X509_NAME_ENTRY *e = kossl->X509_NAME_get_entry(subject, i);
			QString s = e ? nameText(kossl, kossl->X509_NAME_ENTRY_get_data(e)) : QString::null;
			if (!s.isEmpty())
				cns += s;
		}
	}

	for (QStringList::Iterator cn = cns.begin(); cn != cns.end(); ++cn) {
		if (cnMatchesAddress((*cn).stripWhiteSpace().lower()))
			return true;
	}

#endif

	return false;
}


bool KSSLPeerInfo::cnMatchesAddress(QString cn) {
#ifdef KSSL_HAVE_SSL
	QRegExp rx;

	kdDebug(7029) << "Matching CN=[" << cn << "] to ["
		      << d->peerHost << "]" << endl;

	// Check for invalid characters
	if (QRegExp("[^a-zA-Z0-9\\.\\*\\-]").search(cn) >= 0) {
		kdDebug(7029) << "CN contains invalid characters!  Failing." << endl;
		return false;
	}

	// Domains can legally end with '.'s.  We don't need them though.
	while(cn.endsWith("."))
		cn.truncate(cn.length()-1);

	// Do not let empty CN's get by!!
	if (cn.isEmpty())
		return false;

	// Check for IPv4 address
	rx.setPattern("[0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3}");
	if (rx.exactMatch(d->peerHost))
		return d->peerHost == cn;

	// Check for IPv6 address here...
	rx.setPattern("^\\[.*\\]$");
	if (rx.exactMatch(d->peerHost))
		return d->peerHost == cn;

	if (cn.contains('*')) {
		// First make sure that there are at least two valid parts
		// after the wildcard (*).
		QStringList parts = QStringList::split('.', cn, false);

		while (parts.count() > 2)
			parts.remove(parts.begin());

		if (parts.count() != 2) {
			return false;  // we don't allow *.root - that's bad
		}

		if (parts[0].contains('*') || parts[1].contains('*')) {
			return false;
		}

		// RFC2818 says that *.example.com should match against
		// foo.example.com but not bar.foo.example.com
		// (ie. they must have the same number of parts)
		if (QRegExp(cn, false, true).exactMatch(d->peerHost) &&
		    QStringList::split('.', cn, false).count() ==
		    QStringList::split('.', d->peerHost, false).count())
			return true;

		// *.example.com must match example.com also.  Sigh..
		if (cn.startsWith("*.")) {
			QString chopped = cn.mid(2);
			if (chopped == d->peerHost) {
				return true;
			}
		}
		return false;
	}

	// We must have an exact match in this case (insensitive though)
	// (note we already did .lower())
	if (cn == d->peerHost)
		return true;
#endif
	return false;
}


void KSSLPeerInfo::reset() {
	d->peerHost = QString::null;
}


const QString& KSSLPeerInfo::peerHost() const {
	return d->peerHost;
}

