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
#ifdef HAVE_LIBPSL
#include <libpsl.h>
#endif
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
// The binary form of host if it is an IP address: "192.0.2.1", "::1",
// "[::1]", "fe80::1%eth0" or "[fe80::1%eth0]" (a zone id or brackets
// only with IPv6).  Returns its length (4 or 16), or 0.
static int ipAddress(const QString &host, unsigned char *addr)
{
	QString h = host;
	if (h.isEmpty())
		return 0;
	bool v6only = false;
	if (h.startsWith("[") && h.endsWith("]")) {
		h = h.mid(1, h.length() - 2);
		v6only = true;
	}
	int zone = h.find('%');
	if (zone >= 0) {
		h.truncate(zone);
		v6only = true;
	}
	QCString a = h.latin1();
	if (a.isEmpty())
		return 0;
	if (!v6only && inet_pton(AF_INET, a.data(), addr) == 1)
		return 4;
	if (inet_pton(AF_INET6, a.data(), addr) == 1)
		return 16;
	return 0;
}

// Whether the peer host can match at all: an IP address, or a host
// name in ASCII (an IDN as A-labels).  setPeerHost() leaves a host
// whose conversion to ASCII failed empty (a label longer than 63
// characters, ...) or, under a top-level domain not listed in
// KDE_USE_IDN, in Unicode: neither matches anything, as the names of
// the certificate are ASCII only (see nameText()).
static bool usablePeerHost(const QString &host)
{
	unsigned char addr[16];
	if (host.isEmpty())
		return false;
	if (ipAddress(host, addr) > 0)
		return true;
	return QRegExp("[^a-zA-Z0-9\\.\\-]").search(host) < 0;
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

// Whether "*.<suffix>" would cover a whole registry ("*.co.uk",
// "*.tokyo.jp"): with libpsl, if suffix is a public suffix of the ICANN
// section of the Public Suffix List (the system's list if it is newer
// than the one built into libpsl).  The private section (github.io,
// ...) is not used: such names are registered domains for this.
// Without libpsl only the rules of hostMatches() apply.  CAs must not
// issue such names in any case.
static bool isPublicSuffix(const QString &suffix)
{
#ifdef HAVE_LIBPSL
	static const psl_ctx_t *psl = 0;
	static bool loaded = false;
	if (!loaded) {
		loaded = true;
		psl = psl_latest(NULL);
		if (!psl)
			psl = psl_builtin();
	}
	QCString s = suffix.latin1();
	return psl && !s.isEmpty() && psl_is_public_suffix2(psl, s.data(), PSL_TYPE_ICANN);
#else
	Q_UNUSED(suffix);
	return false;
#endif
}

// Matches the presented name (dNSName or CN, ASCII) against the host
// name host (ASCII, IDNs as A-labels), with the wildcard rules of RFC
// 6125 and the CA/Browser Forum: only a whole left-most label "*",
// matching exactly one non-empty label, and at least two labels after
// it, the last of which is not all digits (an IPv4 address) and which
// are not a public suffix ("*.com", "*.localhost", "*.1", and with
// libpsl "*.co.uk" match nothing); "*.example.com" does not match
// "example.com".
static bool hostMatches(QString name, const QString &host)
{
	if (name.isEmpty() || host.isEmpty())
		return false;
	// only characters of host names (and the wildcard)
	if (QRegExp("[^a-zA-Z0-9\\.\\*\\-]").search(name) >= 0)
		return false;
	name = name.lower();
	// one trailing dot (fully qualified) is allowed
	if (name.endsWith("."))
		name.truncate(name.length() - 1);

	QStringList labels = QStringList::split('.', name, true);
	QStringList hostLabels = QStringList::split('.', host.lower(), true);
	for (QStringList::ConstIterator i = labels.begin(); i != labels.end(); ++i) {
		if ((*i).isEmpty())
			return false;
	}
	for (QStringList::ConstIterator i = hostLabels.begin(); i != hostLabels.end(); ++i) {
		if ((*i).isEmpty())
			return false;
	}

	if (!name.contains('*'))
		return name == host.lower();

	if (labels[0] != "*" || name.contains('*') != 1)
		return false;
	if (labels.count() < 3 || QRegExp("[0-9]+").exactMatch(labels.last()))
		return false;
	if (isPublicSuffix(name.mid(2)))
		return false;
	if (labels.count() != hostLabels.count())
		return false;
	for (uint i = 1; i < labels.count(); i++) {
		if (labels[i] != hostLabels[i])
			return false;
	}
	return true;
}

// Legacy: a CN or dNSName holding the text of an IPv4 address matches
// a peer with that address (RFC 6125 wants an iPAddress SAN).
static bool ipTextMatches(const QString &name, const unsigned char *addr, int addrLen)
{
	unsigned char a[4];
	return addrLen == 4 && !name.isEmpty() &&
	       inet_pton(AF_INET, name.latin1(), a) == 1 &&
	       memcmp(a, addr, 4) == 0;
}
#endif

// The names the certificate is valid for, as RFC 6125 has them: the
// dNSNames and iPAddresses of the subjectAltName extension, and the
// common names (CN) of the subject only if there is neither (like
// mozilla::pkix; RFC 6125 only requires no dNSName).  The
// common names are taken from the subject entries themselves, not by
// parsing the X509_NAME_oneline() text, in which another entry can
// fake one ("O=x/CN=localhost").  A subjectAltName extension that
// cannot be decoded (or is there twice) matches nothing.  A peer that
// is an IP address matches an iPAddress, and only if the certificate
// has no iPAddress at all, a dNSName or CN with its text (IPv4 only,
// as before); wildcards never match an IP address.
bool KSSLPeerInfo::certMatchesAddress() {
#ifdef KSSL_HAVE_SSL
	KOpenSSLProxy *kossl = KOpenSSLProxy::self();
	X509 *x = m_cert.getCert();
	if (!x || !usablePeerHost(d->peerHost))
		return false;

	unsigned char addr[16];
	int addrLen = ipAddress(d->peerHost, addr);

	QStringList names;
	bool haveDNSName = false;
	bool haveIPAddress = false;
	if (kossl->X509_get_ext_by_NID(x, NID_subject_alt_name, -1) >= 0) {
		int crit = 0;
		GENERAL_NAMES *gens = (GENERAL_NAMES *)kossl->X509_get_ext_d2i(x, NID_subject_alt_name, &crit, 0);
		if (!gens)
			return false;	// undecodable or duplicate: fail closed
		int cnt = kossl->OPENSSL_sk_num((OPENSSL_STACK *)gens);
		bool ipMatch = false;
		for (int i = 0; i < cnt; i++) {
			const GENERAL_NAME *val = (const GENERAL_NAME *)kossl->OPENSSL_sk_value((OPENSSL_STACK *)gens, i);
			if (val->type == GEN_DNS) {
				// even one that is skipped below
				haveDNSName = true;
				QString s = nameText(kossl, val->d.ia5);
				if (!s.isEmpty())
					names += s;
			} else if (val->type == GEN_IPADD) {
				haveIPAddress = true;
				if (addrLen > 0 &&
				    kossl->ASN1_STRING_length(val->d.ip) == addrLen &&
				    memcmp(kossl->ASN1_STRING_data(val->d.ip), addr, addrLen) == 0)
					ipMatch = true;
			}
		}
		kossl->GENERAL_NAMES_free(gens);
		if (ipMatch)
			return true;
	}

	if (!haveDNSName && !haveIPAddress) {
		X509_NAME *subject = kossl->X509_get_subject_name(x);
		for (int i = -1; subject && (i = kossl->X509_NAME_get_index_by_NID(subject, NID_commonName, i)) >= 0; ) {
			X509_NAME_ENTRY *e = kossl->X509_NAME_get_entry(subject, i);
			QString s = e ? nameText(kossl, kossl->X509_NAME_ENTRY_get_data(e)) : QString::null;
			if (!s.isEmpty())
				names += s;
		}
	}

	for (QStringList::ConstIterator name = names.begin(); name != names.end(); ++name) {
		if (addrLen > 0) {
			if (!haveIPAddress && ipTextMatches(*name, addr, addrLen))
				return true;
		} else if (hostMatches(*name, d->peerHost)) {
			return true;
		}
	}
#endif

	return false;
}


// The rules of certMatchesAddress() for one name.
bool KSSLPeerInfo::cnMatchesAddress(QString cn) {
#ifdef KSSL_HAVE_SSL
	kdDebug(7029) << "Matching CN=[" << cn << "] to ["
		      << d->peerHost << "]" << endl;

	if (!usablePeerHost(d->peerHost))
		return false;
	// ASCII only, see nameText()
	for (uint i = 0; i < cn.length(); i++) {
		if (cn[i].unicode() == 0 || cn[i].unicode() > 0x7f)
			return false;
	}
	unsigned char addr[16];
	int addrLen = ipAddress(d->peerHost, addr);
	if (addrLen > 0)
		return ipTextMatches(cn, addr, addrLen);
	return hostMatches(cn, d->peerHost);
#else
	Q_UNUSED(cn);
	return false;
#endif
}


void KSSLPeerInfo::reset() {
	d->peerHost = QString::null;
}


const QString& KSSLPeerInfo::peerHost() const {
	return d->peerHost;
}

