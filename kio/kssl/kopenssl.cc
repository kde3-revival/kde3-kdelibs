/* This file is part of the KDE libraries
   Copyright (C) 2001-2003 George Staikos <staikos@kde.org>

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

// KOpenSSLProxy used to load libssl and libcrypto at run time with
// dlopen().  kdelibs links OpenSSL (>= 3.0) directly now and calls it
// itself; this class stays only for applications built against it
// (it is exported and its header is installed): every method calls the
// OpenSSL function of the same name.  Functions that OpenSSL 3 no
// longer has (SSLv2/SSLv3 methods, EGD, ASN1_HEADER, ...) keep the
// result the proxy returned when it could not resolve them.
//
// Some methods forward to functions that are deprecated in OpenSSL 3
// (RSA_generate_key(), RSA_free(), EVP_PKEY_get0_RSA() ...); they are
// part of this class's API, so the warnings are switched off here, for
// this file only.
#define OPENSSL_SUPPRESS_DEPRECATED

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <kstaticdeleter.h>

#include "kopenssl.h"

#ifdef KSSL_HAVE_SSL
#include <openssl/err.h>
#include <openssl/rsa.h>
#include <openssl/dsa.h>
#endif

KOpenSSLProxy* KOpenSSLProxy::_me = 0L;
static KStaticDeleter<KOpenSSLProxy> medProxy;


KOpenSSLProxy::KOpenSSLProxy()
   : d(0L), _sslLib(0L), _cryptoLib(0L), _ok(true) {
#ifdef KSSL_HAVE_SSL
   // OpenSSL initialises itself on first use; this only makes sure the
   // configuration file (system crypto policy) is loaded as before.
   OPENSSL_init_ssl(OPENSSL_INIT_LOAD_CONFIG, NULL);
#endif
}


KOpenSSLProxy::~KOpenSSLProxy() {
   medProxy.setObject(0);
}


KOpenSSLProxy *KOpenSSLProxy::self() {
#ifdef KSSL_HAVE_SSL
   if (!_me) {
      _me = medProxy.setObject(_me, new KOpenSSLProxy);
   }
#endif
   return _me;
}


// libssl and libcrypto are linked: always there.
bool KOpenSSLProxy::hasLibSSL() const {
   return true;
}


bool KOpenSSLProxy::hasLibCrypto() const {
   return true;
}


void KOpenSSLProxy::destroy() {
  delete this;
  _me = 0L;
}


#ifdef KSSL_HAVE_SSL

int KOpenSSLProxy::SSL_connect(SSL *ssl) {
   return ::SSL_connect(ssl);
}


int KOpenSSLProxy::SSL_accept(SSL *ssl) {
   return ::SSL_accept(ssl);
}


int KOpenSSLProxy::SSL_read(SSL *ssl, void *buf, int num) {
   return ::SSL_read(ssl, buf, num);
}


int KOpenSSLProxy::SSL_write(SSL *ssl, const void *buf, int num) {
   return ::SSL_write(ssl, buf, num);
}


SSL *KOpenSSLProxy::SSL_new(SSL_CTX *ctx) {
   return ::SSL_new(ctx);
}


void KOpenSSLProxy::SSL_free(SSL *ssl) {
   ::SSL_free(ssl);
}


int KOpenSSLProxy::SSL_shutdown(SSL *ssl) {
   return ::SSL_shutdown(ssl);
}


SSL_CTX *KOpenSSLProxy::SSL_CTX_new(SSL_METHOD *method) {
   return ::SSL_CTX_new(method);
}


void KOpenSSLProxy::SSL_CTX_free(SSL_CTX *ctx) {
   ::SSL_CTX_free(ctx);
}


int KOpenSSLProxy::SSL_set_fd(SSL *ssl, int fd) {
   return ::SSL_set_fd(ssl, fd);
}


int KOpenSSLProxy::SSL_pending(SSL *ssl) {
   return ::SSL_pending(ssl);
}


int KOpenSSLProxy::SSL_CTX_set_cipher_list(SSL_CTX *ctx, const char *str) {
   return ::SSL_CTX_set_cipher_list(ctx, str);
}


void KOpenSSLProxy::SSL_CTX_set_verify(SSL_CTX *ctx, int mode,
                              int (*verify_callback)(int, X509_STORE_CTX *)) {
   ::SSL_CTX_set_verify(ctx, mode, verify_callback);
}


int KOpenSSLProxy::SSL_use_certificate(SSL *ssl, X509 *x) {
   return ::SSL_use_certificate(ssl, x);
}


SSL_CIPHER *KOpenSSLProxy::SSL_get_current_cipher(SSL *ssl) {
   return const_cast<SSL_CIPHER *>(::SSL_get_current_cipher(ssl));
}


long KOpenSSLProxy::SSL_ctrl(SSL *ssl,int cmd, long larg, char *parg) {
   return ::SSL_ctrl(ssl, cmd, larg, parg);
}


// These two were SSL_ctrl() macros, so they returned -1 without the library.
uint64_t KOpenSSLProxy::SSL_set_options(SSL *ssl, uint64_t options) {
   return ::SSL_set_options(ssl, options);
}


int KOpenSSLProxy::SSL_session_reused(SSL *ssl) {
   return ::SSL_session_reused(ssl);
}


int KOpenSSLProxy::RAND_egd(const char *path) {
#ifndef OPENSSL_NO_EGD
   return ::RAND_egd(path);
#else
   (void)path;
   return -1;
#endif
}


SSL_METHOD *KOpenSSLProxy::TLSv1_client_method() {
   return const_cast<SSL_METHOD *>(::TLS_client_method());
}


SSL_METHOD *KOpenSSLProxy::SSLv2_client_method() {
   return 0L;   // gone since OpenSSL 1.1.0
}


SSL_METHOD *KOpenSSLProxy::SSLv3_client_method() {
   return 0L;   // gone in OpenSSL 3 builds without SSLv3
}


SSL_METHOD *KOpenSSLProxy::SSLv23_client_method() {
   return const_cast<SSL_METHOD *>(::TLS_client_method());
}


X509 *KOpenSSLProxy::SSL_get_peer_certificate(SSL *s) {
   return ::SSL_get1_peer_certificate(s);
}


int KOpenSSLProxy::SSL_CIPHER_get_bits(const SSL_CIPHER *c,int *alg_bits) {
   return ::SSL_CIPHER_get_bits(c, alg_bits);
}


char * KOpenSSLProxy::SSL_CIPHER_get_version(const SSL_CIPHER *c) {
   return const_cast<char *>(::SSL_CIPHER_get_version(c));
}


const char * KOpenSSLProxy::SSL_CIPHER_get_name(const SSL_CIPHER *c) {
   return ::SSL_CIPHER_get_name(c);
}


char * KOpenSSLProxy::SSL_CIPHER_description(const SSL_CIPHER *c,char *buf,int size) {
   return ::SSL_CIPHER_description(c,buf,size);
}


X509 * KOpenSSLProxy::d2i_X509(X509 **a,unsigned char **pp,long length) {
   return ::d2i_X509(a, const_cast<const unsigned char **>(pp),length);
}


int KOpenSSLProxy::i2d_X509(X509 *a,unsigned char **pp) {
   return ::i2d_X509(a,pp);
}


int KOpenSSLProxy::X509_cmp(X509 *a, X509 *b) {
   return ::X509_cmp(a,b);
}


X509_STORE *KOpenSSLProxy::X509_STORE_new(void) {
   return ::X509_STORE_new();
}


void KOpenSSLProxy::X509_STORE_free(X509_STORE *v) {
   ::X509_STORE_free(v);
}


X509_STORE_CTX *KOpenSSLProxy::X509_STORE_CTX_new(void) {
   return ::X509_STORE_CTX_new();
}


void KOpenSSLProxy::X509_STORE_CTX_free(X509_STORE_CTX *ctx) {
   ::X509_STORE_CTX_free(ctx);
}


int KOpenSSLProxy::X509_verify_cert(X509_STORE_CTX *ctx) {
   return ::X509_verify_cert(ctx);
}


void KOpenSSLProxy::X509_free(X509 *a) {
   ::X509_free(a);
}


char *KOpenSSLProxy::X509_NAME_oneline(X509_NAME *a,char *buf,int size) {
   return ::X509_NAME_oneline(a,buf,size);
}


X509_NAME *KOpenSSLProxy::X509_get_subject_name(X509 *a) {
   return const_cast<X509_NAME *>(::X509_get_subject_name(a));
}


X509_NAME *KOpenSSLProxy::X509_get_issuer_name(X509 *a) {
   return const_cast<X509_NAME *>(::X509_get_issuer_name(a));
}


int KOpenSSLProxy::X509_NAME_get_index_by_NID(X509_NAME *name, int nid, int lastpos) {
   return ::X509_NAME_get_index_by_NID(name,nid,lastpos);
}


X509_NAME_ENTRY *KOpenSSLProxy::X509_NAME_get_entry(X509_NAME *name, int loc) {
   return const_cast<X509_NAME_ENTRY *>(::X509_NAME_get_entry(name,loc));
}


ASN1_STRING *KOpenSSLProxy::X509_NAME_ENTRY_get_data(X509_NAME_ENTRY *ne) {
   return const_cast<ASN1_STRING *>(::X509_NAME_ENTRY_get_data(ne));
}


int KOpenSSLProxy::ASN1_STRING_to_UTF8(unsigned char **out, ASN1_STRING *in) {
   return ::ASN1_STRING_to_UTF8(out,in);
}


X509_LOOKUP *KOpenSSLProxy::X509_STORE_add_lookup(X509_STORE *v, X509_LOOKUP_METHOD *m) {
   return ::X509_STORE_add_lookup(v,m);
}


X509_LOOKUP_METHOD *KOpenSSLProxy::X509_LOOKUP_file(void) {
   return ::X509_LOOKUP_file();
}


void KOpenSSLProxy::X509_LOOKUP_free(X509_LOOKUP *x) {
   ::X509_LOOKUP_free(x);
}


int KOpenSSLProxy::X509_LOOKUP_ctrl(X509_LOOKUP *ctx, int cmd, const char *argc, long argl, char **ret) {
   return ::X509_LOOKUP_ctrl(ctx,cmd,argc,argl,ret);
}


void KOpenSSLProxy::X509_STORE_CTX_init(X509_STORE_CTX *ctx, X509_STORE *store, X509 *x509, STACK_OF(X509) *chain) {
   ::X509_STORE_CTX_init(ctx,store,x509,chain);
}


void KOpenSSLProxy::CRYPTO_free(void *x, const char *file, int line) {
   ::CRYPTO_free(x, file, line);
}


X509 *KOpenSSLProxy::X509_dup(X509 *x509) {
   return ::X509_dup(x509);
}


BIO *KOpenSSLProxy::BIO_new(BIO_METHOD *type) {
   return ::BIO_new(type);
}


BIO_METHOD *KOpenSSLProxy::BIO_s_mem(void) {
   return const_cast<BIO_METHOD *>(::BIO_s_mem());
}


BIO *KOpenSSLProxy::BIO_new_fp(FILE *stream, int close_flag) {
   return ::BIO_new_fp(stream, close_flag);
}


BIO *KOpenSSLProxy::BIO_new_mem_buf(void *buf, int len) {
   return ::BIO_new_mem_buf(buf,len);
}


int KOpenSSLProxy::BIO_free(BIO *a) {
   return ::BIO_free(a);
}


long KOpenSSLProxy::BIO_ctrl(BIO *bp,int cmd,long larg,void *parg) {
   return ::BIO_ctrl(bp,cmd,larg,parg);
}


int KOpenSSLProxy::BIO_write(BIO *b, const void *data, int len) {
   return ::BIO_write(b, data, len);
}


int KOpenSSLProxy::PEM_write_bio_X509(BIO *bp, X509 *x) {
   return ::PEM_write_bio_X509(bp, x);
}


int KOpenSSLProxy::ASN1_i2d_fp(FILE *out,unsigned char *x) {
   // wrote an ASN1_HEADER, which is gone since OpenSSL 0.9.8
   (void)out; (void)x;
   return -1;
}


int KOpenSSLProxy::X509_print(FILE *fp, X509 *x) {
   return ::X509_print_fp(fp, x);
}


PKCS12 *KOpenSSLProxy::d2i_PKCS12_fp(FILE *fp, PKCS12 **p12) {
   return ::d2i_PKCS12_fp(fp, p12);
}


int KOpenSSLProxy::PKCS12_newpass(PKCS12 *p12, char *oldpass, char *newpass) {
   return ::PKCS12_newpass(p12, oldpass, newpass);
}


int KOpenSSLProxy::i2d_PKCS12(PKCS12 *p12, unsigned char **p) {
   return ::i2d_PKCS12(p12, p);
}


int KOpenSSLProxy::i2d_PKCS12_fp(FILE *fp, PKCS12 *p12) {
   return ::i2d_PKCS12_fp(fp, p12);
}


PKCS12 *KOpenSSLProxy::PKCS12_new(void) {
   return ::PKCS12_new();
}


void KOpenSSLProxy::PKCS12_free(PKCS12 *a) {
   ::PKCS12_free(a);
}


int KOpenSSLProxy::PKCS12_parse(PKCS12 *p12, const char *pass, EVP_PKEY **pkey,
                    X509 **cert, STACK_OF(X509) **ca) {
   return ::PKCS12_parse(p12, pass, pkey, cert, ca);
}


void KOpenSSLProxy::EVP_PKEY_free(EVP_PKEY *x) {
   ::EVP_PKEY_free(x);
}


EVP_PKEY* KOpenSSLProxy::EVP_PKEY_new() {
   return ::EVP_PKEY_new();
}


void KOpenSSLProxy::X509_REQ_free(X509_REQ *x) {
   ::X509_REQ_free(x);
}


X509_REQ* KOpenSSLProxy::X509_REQ_new() {
   return ::X509_REQ_new();
}


int KOpenSSLProxy::SSL_CTX_use_PrivateKey(SSL_CTX *ctx, EVP_PKEY *pkey) {
   return ::SSL_CTX_use_PrivateKey(ctx,pkey);
}


int KOpenSSLProxy::SSL_CTX_use_certificate(SSL_CTX *ctx, X509 *x) {
   return ::SSL_CTX_use_certificate(ctx,x);
}


int KOpenSSLProxy::SSL_get_error(SSL *ssl, int rc) {
   return ::SSL_get_error(ssl,rc);
}


STACK_OF(X509) *KOpenSSLProxy::SSL_get_peer_cert_chain(SSL *s) {
   return ::SSL_get_peer_cert_chain(s);
}


void KOpenSSLProxy::OPENSSL_sk_free(OPENSSL_STACK *s) {
   ::OPENSSL_sk_free(s);
}


int KOpenSSLProxy::OPENSSL_sk_num(const OPENSSL_STACK *s) {
   return ::OPENSSL_sk_num(s);
}


char *KOpenSSLProxy::OPENSSL_sk_pop(OPENSSL_STACK *s) {
   return static_cast<char *>(::OPENSSL_sk_pop(s));
}


char *KOpenSSLProxy::OPENSSL_sk_value(const OPENSSL_STACK *s, int n) {
   return static_cast<char *>(::OPENSSL_sk_value(s, n));
}


void KOpenSSLProxy::X509_STORE_CTX_set_chain(X509_STORE_CTX *v, STACK_OF(X509)* x) {
   ::X509_STORE_CTX_set0_untrusted(v,x);
}

void KOpenSSLProxy::X509_STORE_CTX_set_purpose(X509_STORE_CTX *v, int purpose) {
   ::X509_STORE_CTX_set_purpose(v,purpose);
}


void KOpenSSLProxy::X509_STORE_CTX_cleanup(X509_STORE_CTX *v) {
   ::X509_STORE_CTX_cleanup(v);
}


// The following replace direct accesses to the members of X509_STORE_CTX
// (error, error_depth, current_cert) and X509_STORE (verify_cb)
int KOpenSSLProxy::X509_STORE_CTX_get_error(X509_STORE_CTX *v) {
   return ::X509_STORE_CTX_get_error(v);
}


void KOpenSSLProxy::X509_STORE_CTX_set_error(X509_STORE_CTX *v, int s) {
   ::X509_STORE_CTX_set_error(v, s);
}


int KOpenSSLProxy::X509_STORE_CTX_get_error_depth(X509_STORE_CTX *v) {
   return ::X509_STORE_CTX_get_error_depth(v);
}


X509 *KOpenSSLProxy::X509_STORE_CTX_get_current_cert(X509_STORE_CTX *v) {
   return ::X509_STORE_CTX_get_current_cert(v);
}


void KOpenSSLProxy::X509_STORE_set_verify_cb(X509_STORE *v, int (*verify_cb)(int, X509_STORE_CTX *)) {
   ::X509_STORE_set_verify_cb(v, verify_cb);
}


int KOpenSSLProxy::X509_STORE_set_default_paths(X509_STORE *v) {
   return ::X509_STORE_set_default_paths(v);
}


int KOpenSSLProxy::X509_STORE_add_cert(X509_STORE *v, X509 *x) {
   return ::X509_STORE_add_cert(v, x);
}


STACK_OF(X509_OBJECT) *KOpenSSLProxy::X509_STORE_get0_objects(X509_STORE *v) {
   return ::X509_STORE_get0_objects(v);
}


int KOpenSSLProxy::X509_OBJECT_get_type(const X509_OBJECT *a) {
   return ::X509_OBJECT_get_type(a);
}


X509 *KOpenSSLProxy::X509_OBJECT_get0_X509(const X509_OBJECT *a) {
   return ::X509_OBJECT_get0_X509(a);
}


STACK_OF(X509) *KOpenSSLProxy::X509_STORE_CTX_get0_chain(X509_STORE_CTX *v) {
   return ::X509_STORE_CTX_get0_chain(v);
}


OPENSSL_STACK* KOpenSSLProxy::OPENSSL_sk_dup(const OPENSSL_STACK *s) {
   return ::OPENSSL_sk_dup(s);
}


OPENSSL_STACK* KOpenSSLProxy::OPENSSL_sk_new(OPENSSL_sk_compfunc cmp) {
   return ::OPENSSL_sk_new(cmp);
}


int KOpenSSLProxy::OPENSSL_sk_push(OPENSSL_STACK* s, const void* d) {
   return ::OPENSSL_sk_push(s,d);
}


char *KOpenSSLProxy::i2s_ASN1_INTEGER(X509V3_EXT_METHOD *meth, ASN1_INTEGER *aint) {
   return ::i2s_ASN1_INTEGER(meth, aint);
}


ASN1_INTEGER *KOpenSSLProxy::X509_get_serialNumber(X509 *x) {
   return ::X509_get_serialNumber(x);
}


EVP_PKEY *KOpenSSLProxy::X509_get_pubkey(X509 *x) {
   return ::X509_get_pubkey(x);
}


// The following replace direct accesses to the members of X509, EVP_PKEY,
// RSA and DSA
ASN1_TIME *KOpenSSLProxy::X509_getm_notBefore(const X509 *x) {
   return ::X509_getm_notBefore(const_cast<X509 *>(x));
}


ASN1_TIME *KOpenSSLProxy::X509_getm_notAfter(const X509 *x) {
   return ::X509_getm_notAfter(const_cast<X509 *>(x));
}


int KOpenSSLProxy::X509_get_signature_nid(const X509 *x) {
   return ::X509_get_signature_nid(x);
}


void KOpenSSLProxy::X509_get0_signature(const ASN1_BIT_STRING **psig, const X509_ALGOR **palg, const X509 *x) {
   ::X509_get0_signature(psig, palg, x);
}


int KOpenSSLProxy::EVP_PKEY_get_id(const EVP_PKEY *pkey) {
   return ::EVP_PKEY_get_id(pkey);
}


RSA *KOpenSSLProxy::EVP_PKEY_get0_RSA(EVP_PKEY *pkey) {
   return const_cast<RSA *>(::EVP_PKEY_get0_RSA(pkey));
}


DSA *KOpenSSLProxy::EVP_PKEY_get0_DSA(EVP_PKEY *pkey) {
   return const_cast<DSA *>(::EVP_PKEY_get0_DSA(pkey));
}


void KOpenSSLProxy::RSA_get0_key(const RSA *r, const BIGNUM **n, const BIGNUM **e, const BIGNUM **d) {
   ::RSA_get0_key(r, n, e, d);
}


void KOpenSSLProxy::DSA_get0_pqg(const DSA *d, const BIGNUM **p, const BIGNUM **q, const BIGNUM **g) {
   ::DSA_get0_pqg(d, p, q, g);
}


void KOpenSSLProxy::DSA_get0_key(const DSA *d, const BIGNUM **pub_key, const BIGNUM **priv_key) {
   ::DSA_get0_key(d, pub_key, priv_key);
}


int KOpenSSLProxy::i2d_PublicKey(EVP_PKEY *a, unsigned char **pp) {
   return ::i2d_PublicKey(a,pp);
}


int KOpenSSLProxy::X509_check_private_key(X509 *x, EVP_PKEY *p) {
   return ::X509_check_private_key(x,p);
}


char *KOpenSSLProxy::BN_bn2hex(const BIGNUM *a) {
   return ::BN_bn2hex(a);
}


int KOpenSSLProxy::X509_digest(const X509 *x,const EVP_MD *t, unsigned char *md, unsigned int *len) {
   return ::X509_digest(x, t, md, len);
}


EVP_MD *KOpenSSLProxy::EVP_md5() {
   return const_cast<EVP_MD *>(::EVP_md5());
}


void KOpenSSLProxy::ASN1_INTEGER_free(ASN1_INTEGER *a) {
   ::ASN1_INTEGER_free(a);
}


int KOpenSSLProxy::OBJ_obj2nid(ASN1_OBJECT *o) {
   return ::OBJ_obj2nid(o);
}


const char * KOpenSSLProxy::OBJ_nid2ln(int n) {
   return ::OBJ_nid2ln(n);
}


int KOpenSSLProxy::X509_get_ext_count(X509 *x) {
   return ::X509_get_ext_count(x);
}


int KOpenSSLProxy::X509_get_ext_by_NID(X509 *x, int nid, int lastpos) {
   return ::X509_get_ext_by_NID(x,nid,lastpos);
}


int KOpenSSLProxy::X509_get_ext_by_OBJ(X509 *x,ASN1_OBJECT *obj,int lastpos) {
   return ::X509_get_ext_by_OBJ(x,obj,lastpos);
}


X509_EXTENSION *KOpenSSLProxy::X509_get_ext(X509 *x, int loc) {
   return const_cast<X509_EXTENSION *>(::X509_get_ext(x,loc));
}


X509_EXTENSION *KOpenSSLProxy::X509_delete_ext(X509 *x, int loc) {
   return ::X509_delete_ext(x,loc);
}


int KOpenSSLProxy::X509_add_ext(X509 *x, X509_EXTENSION *ex, int loc) {
   return ::X509_add_ext(x,ex,loc);
}


void *KOpenSSLProxy::X509_get_ext_d2i(X509 *x, int nid, int *crit, int *idx) {
   return ::X509_get_ext_d2i(x,nid,crit,idx);
}


void KOpenSSLProxy::GENERAL_NAMES_free(GENERAL_NAMES *gens) {
   ::GENERAL_NAMES_free(gens);
}


char *KOpenSSLProxy::i2s_ASN1_OCTET_STRING(X509V3_EXT_METHOD *method, ASN1_OCTET_STRING *ia5) {
   return ::i2s_ASN1_OCTET_STRING(method,ia5);
}


int KOpenSSLProxy::ASN1_BIT_STRING_get_bit(ASN1_BIT_STRING *a, int n) {
   return ::ASN1_BIT_STRING_get_bit(a,n);
}


PKCS7 *KOpenSSLProxy::PKCS7_new(void) {
   return ::PKCS7_new();
}


void KOpenSSLProxy::PKCS7_free(PKCS7 *a) {
   ::PKCS7_free(a);
}


void KOpenSSLProxy::PKCS7_content_free(PKCS7 *a) {
   (void)a;     // gone since OpenSSL 0.9.8; was a no-op without it
}


int KOpenSSLProxy::i2d_PKCS7(PKCS7 *a, unsigned char **pp) {
   return ::i2d_PKCS7(a,pp);
}


PKCS7 *KOpenSSLProxy::d2i_PKCS7(PKCS7 **a, unsigned char **pp,long length) {
   return ::d2i_PKCS7(a, const_cast<const unsigned char **>(pp),length);
}


int KOpenSSLProxy::i2d_PKCS7_fp(FILE *fp,PKCS7 *p7) {
   return ::i2d_PKCS7_fp(fp,p7);
}


PKCS7 *KOpenSSLProxy::d2i_PKCS7_fp(FILE *fp,PKCS7 **p7) {
   return ::d2i_PKCS7_fp(fp,p7);
}


int KOpenSSLProxy::i2d_PKCS7_bio(BIO *bp,PKCS7 *p7) {
   return ::i2d_PKCS7_bio(bp, p7);
}


PKCS7 *KOpenSSLProxy::d2i_PKCS7_bio(BIO *bp,PKCS7 **p7) {
   return ::d2i_PKCS7_bio(bp, p7);
}


PKCS7 *KOpenSSLProxy::PKCS7_dup(PKCS7 *p7) {
   return ::PKCS7_dup(p7);
}


PKCS7 *KOpenSSLProxy::PKCS7_sign(X509 *signcert, EVP_PKEY *pkey, STACK_OF(X509) *certs,
				 BIO *data, int flags) {
   return ::PKCS7_sign(signcert,pkey,certs,data,flags);
}


int KOpenSSLProxy::PKCS7_verify(PKCS7* p, STACK_OF(X509)* st, X509_STORE* s, BIO* in, BIO *out, int flags) {
   return ::PKCS7_verify(p,st,s,in,out,flags);
}


STACK_OF(X509) *KOpenSSLProxy::PKCS7_get0_signers(PKCS7 *p7, STACK_OF(X509) *certs, int flags) {
   return ::PKCS7_get0_signers(p7,certs,flags);
}


PKCS7 *KOpenSSLProxy::PKCS7_encrypt(STACK_OF(X509) *certs, BIO *in, EVP_CIPHER *cipher,
				    int flags) {
   return ::PKCS7_encrypt(certs,in,cipher,flags);
}


int KOpenSSLProxy::PKCS7_decrypt(PKCS7 *p7, EVP_PKEY *pkey, X509 *cert, BIO *data, int flags) {
   return ::PKCS7_decrypt(p7,pkey,cert,data,flags);
}


STACK_OF(X509_NAME) *KOpenSSLProxy::SSL_load_client_CA_file(const char *file) {
   return ::SSL_load_client_CA_file(file);
}


STACK_OF(X509_INFO) *KOpenSSLProxy::PEM_X509_INFO_read(FILE *fp, STACK_OF(X509_INFO) *sk, pem_password_cb *cb, void *u) {
   return ::PEM_X509_INFO_read(fp,sk,cb,u);
}


X509 *KOpenSSLProxy::X509_d2i_fp(FILE *out, X509** buf) {
   return ::d2i_X509_fp(out, buf);
}


int KOpenSSLProxy::SSL_peek(SSL *ssl,void *buf,int num) {
   return ::SSL_peek(ssl,buf,num);
}


const char *KOpenSSLProxy::RAND_file_name(char *buf, size_t num) {
   return ::RAND_file_name(buf, num);
}


int KOpenSSLProxy::RAND_load_file(const char *filename, long max_bytes) {
   return ::RAND_load_file(filename, max_bytes);
}


int KOpenSSLProxy::RAND_write_file(const char *filename) {
   return ::RAND_write_file(filename);
}


int KOpenSSLProxy::X509_PURPOSE_get_count() {
   return ::X509_PURPOSE_get_count();
}


int KOpenSSLProxy::X509_PURPOSE_get_id(X509_PURPOSE *p) {
   return ::X509_PURPOSE_get_id(p);
}


int KOpenSSLProxy::X509_check_purpose(X509 *x, int id, int ca) {
   return ::X509_check_purpose(x, id, ca);
}


X509_PURPOSE *KOpenSSLProxy::X509_PURPOSE_get0(int idx) {
   return ::X509_PURPOSE_get0(idx);
}


int KOpenSSLProxy::EVP_PKEY_assign(EVP_PKEY *pkey, int type, char *key) {
   return ::EVP_PKEY_assign(pkey, type, key);
}


int KOpenSSLProxy::X509_REQ_set_pubkey(X509_REQ *x, EVP_PKEY *pkey) {
   return ::X509_REQ_set_pubkey(x, pkey);
}


RSA* KOpenSSLProxy::RSA_generate_key(int bits, unsigned long e, void
                        (*callback)(int,int,void *), void *cb_arg) {
   return ::RSA_generate_key(bits, e, callback, cb_arg);
}

OPENSSL_STACK *KOpenSSLProxy::X509_get1_email(X509 *x) {
   return reinterpret_cast<OPENSSL_STACK *>(::X509_get1_email(x));
}

void KOpenSSLProxy::X509_email_free(OPENSSL_STACK *sk) {
   ::X509_email_free(reinterpret_cast<STACK_OF(OPENSSL_STRING) *>(sk));
}

EVP_CIPHER *KOpenSSLProxy::EVP_des_ede3_cbc() {
   return const_cast<EVP_CIPHER *>(::EVP_des_ede3_cbc());
}

EVP_CIPHER *KOpenSSLProxy::EVP_des_cbc() {
   return const_cast<EVP_CIPHER *>(::EVP_des_cbc());
}

EVP_CIPHER *KOpenSSLProxy::EVP_rc2_cbc() {
   return const_cast<EVP_CIPHER *>(::EVP_rc2_cbc());
}

EVP_CIPHER *KOpenSSLProxy::EVP_rc2_64_cbc() {
   return const_cast<EVP_CIPHER *>(::EVP_rc2_64_cbc());
}

EVP_CIPHER *KOpenSSLProxy::EVP_rc2_40_cbc() {
   return const_cast<EVP_CIPHER *>(::EVP_rc2_40_cbc());
}

int KOpenSSLProxy::i2d_X509_REQ_fp(FILE *fp, X509_REQ *x) {
   return ::i2d_X509_REQ_fp(fp,x);
}


void KOpenSSLProxy::ERR_clear_error() {
   ::ERR_clear_error();
}


unsigned long KOpenSSLProxy::ERR_get_error() {
   return ::ERR_get_error();
}


void KOpenSSLProxy::ERR_print_errors_fp(FILE* fp) {
   ::ERR_print_errors_fp(fp);
}


SSL_SESSION *KOpenSSLProxy::SSL_get1_session(SSL *ssl) {
   return ::SSL_get1_session(ssl);
}


void KOpenSSLProxy::SSL_SESSION_free(SSL_SESSION *session) {
   ::SSL_SESSION_free(session);
}


int KOpenSSLProxy::SSL_SESSION_up_ref(SSL_SESSION *session) {
   return ::SSL_SESSION_up_ref(session);
}


int KOpenSSLProxy::SSL_set_session(SSL *ssl, SSL_SESSION *session) {
   return ::SSL_set_session(ssl, session);
}


SSL_SESSION *KOpenSSLProxy::d2i_SSL_SESSION(SSL_SESSION **a, unsigned char **pp, long length) {
   return ::d2i_SSL_SESSION(a, const_cast<const unsigned char **>(pp), length);
}


int KOpenSSLProxy::i2d_SSL_SESSION(SSL_SESSION *in, unsigned char **pp) {
   return ::i2d_SSL_SESSION(in, pp);
}


int KOpenSSLProxy::i2d_PrivateKey_fp(FILE *fp, EVP_PKEY *p) {
   return ::i2d_PrivateKey_fp(fp, p);
}


int KOpenSSLProxy::i2d_PKCS8PrivateKey_fp(FILE *fp, EVP_PKEY *p, const EVP_CIPHER *c, char *k, int klen, pem_password_cb *cb, void *u) {
   return ::i2d_PKCS8PrivateKey_fp(fp, p, c, k, klen, cb, u);
}


void KOpenSSLProxy::RSA_free(RSA *rsa) {
   ::RSA_free(rsa);
}


EVP_CIPHER *KOpenSSLProxy::EVP_bf_cbc() {
   return const_cast<EVP_CIPHER *>(::EVP_bf_cbc());
}


int KOpenSSLProxy::X509_REQ_sign(X509_REQ *x, EVP_PKEY *pkey, const EVP_MD *md) {
   return ::X509_REQ_sign(x, pkey, md);
}


int KOpenSSLProxy::X509_NAME_add_entry_by_txt(X509_NAME *name, char *field,
                   int type, unsigned char *bytes, int len, int loc, int set) {
   return ::X509_NAME_add_entry_by_txt(name, field, type, bytes, len, loc, set);
}


X509_NAME *KOpenSSLProxy::X509_NAME_new() {
   return ::X509_NAME_new();
}


int KOpenSSLProxy::X509_REQ_set_subject_name(X509_REQ *req,X509_NAME *name) {
   return ::X509_REQ_set_subject_name(req, name);
}


unsigned char *KOpenSSLProxy::ASN1_STRING_data(ASN1_STRING *x) {
   return const_cast<unsigned char *>(::ASN1_STRING_get0_data(x));
}


int KOpenSSLProxy::ASN1_STRING_length(const ASN1_STRING *x) {
   return ::ASN1_STRING_length(x);
}


int KOpenSSLProxy::ASN1_STRING_type(const ASN1_STRING *x) {
   return ::ASN1_STRING_type(x);
}

STACK_OF(SSL_CIPHER) *KOpenSSLProxy::SSL_get_ciphers(const SSL* ssl) {
   return ::SSL_get_ciphers(ssl);
}

#endif

