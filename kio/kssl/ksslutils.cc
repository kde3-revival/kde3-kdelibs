/* This file is part of the KDE project
 *
 * Copyright (C) 2000,2001 George Staikos <staikos@kde.org>
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


#include "ksslutils.h"

#include <qstring.h>
#include <kglobal.h>
#include <klocale.h>
#include <qdatetime.h>

#include "ksslopenssl_p.h"

#ifdef KSSL_HAVE_SSL
// This code is mostly taken from OpenSSL v0.9.5a
// by Eric Young
//
// Certificates use UTCTime (YYMMDDHHMM[SS]Z) for dates up to 2049 and
// GeneralizedTime (YYYYMMDDHHMM[SS]Z) from 2050 on (RFC 5280); both
// come as an ASN1_TIME, which is what this gets.
QDateTime ASN1_UTCTIME_QDateTime(ASN1_UTCTIME *tm, int *isGmt) {
QDateTime qdt;
const char *v, *p;
int gmt=0;
int i, len, ylen;
int y=0,M=0,d=0,h=0,m=0,s=0;
QDate qdate;
QTime qtime;

  len = kssl_ASN1_STRING_length(tm);
  v = (const char *)ASN1_STRING_get0_data(tm);
  ylen = (ASN1_STRING_type(tm) == V_ASN1_GENERALIZEDTIME) ? 4 : 2;

  if (!v || len < ylen + 8) goto auq_err;
  if (v[len-1] == 'Z') gmt=1;
  for (i=0; i<ylen+8; i++)
          if ((v[i] > '9') || (v[i] < '0')) goto auq_err;
  if (ylen == 4) {
          y = (v[0]-'0')*1000+(v[1]-'0')*100+(v[2]-'0')*10+(v[3]-'0');
  } else {
          y = (v[0]-'0')*10+(v[1]-'0');
          y += (y < 50) ? 2000 : 1900;
  }
  p = v + ylen;
  M = (p[0]-'0')*10+(p[1]-'0');
  if ((M > 12) || (M < 1)) goto auq_err;
  d = (p[2]-'0')*10+(p[3]-'0');
  h = (p[4]-'0')*10+(p[5]-'0');
  m =  (p[6]-'0')*10+(p[7]-'0');
  if (    len >= ylen + 10 &&
          (p[8] >= '0') && (p[8] <= '9') &&
          (p[9] >= '0') && (p[9] <= '9'))
          s = (p[8]-'0')*10+(p[9]-'0');

  // localize the date and display it.
  qdate.setYMD(y, M, d);
  qtime.setHMS(h,m,s);
  qdt.setDate(qdate); qdt.setTime(qtime);
  auq_err:
  if (isGmt) *isGmt = gmt;
return qdt;
}


QString ASN1_UTCTIME_QString(ASN1_UTCTIME *tm) {
  QString qstr;
  int gmt;
  QDateTime qdt = ASN1_UTCTIME_QDateTime(tm, &gmt);

  qstr = KGlobal::locale()->formatDateTime(qdt, false, true);
  if (gmt) { 
    qstr += " ";
    qstr += i18n("GMT");
  }
  return qstr;
}


QString ASN1_INTEGER_QString(ASN1_INTEGER *aint) {
char *rep = i2s_ASN1_INTEGER(NULL, aint);
QString yy = rep;
OPENSSL_free(rep);
return yy;
}


#endif

