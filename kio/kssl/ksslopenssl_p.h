/* This file is part of the KDE libraries

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License version 2 as published by the Free Software Foundation.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

// Private to kssl (not installed): the OpenSSL headers kssl uses and
// small helpers for API differences between OpenSSL 3.x and 4.x.

#ifndef KSSLOPENSSL_P_H
#define KSSLOPENSSL_P_H

#include "ksslconfig.h"

// this hack provided by Malte Starostik to avoid glibc/openssl bug
// on some systems
#define crypt _openssl_crypt
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/x509_vfy.h>
#include <openssl/pem.h>
#include <openssl/bio.h>
#include <openssl/rand.h>
#include <openssl/asn1.h>
#include <openssl/pkcs7.h>
#include <openssl/pkcs12.h>
#include <openssl/evp.h>
#include <openssl/bn.h>
#include <openssl/core_names.h>
#undef crypt

// The length of an ASN1_STRING: ASN1_STRING_length() is deprecated
// since OpenSSL 4.1 for ASN1_STRING_get_length(), which 3.x lacks.
static inline int kssl_ASN1_STRING_length(const ASN1_STRING *s)
{
#if OPENSSL_VERSION_NUMBER >= 0x40100000L
	return static_cast<int>(ASN1_STRING_get_length(s));
#else
	return ASN1_STRING_length(s);
#endif
}

#endif
