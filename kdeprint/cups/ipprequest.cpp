/*
 *  This file is part of the KDE libraries
 *  Copyright (c) 2001 Michael Goffioul <kdeprint@swing.be>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License version 2 as published by the Free Software Foundation.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 **/

#include "ipprequest.h"
#include "cupsinfos.h"

#include <stdlib.h>
#include <cups/language.h>
#include <kdebug.h>
#include <kglobal.h>
#include <klocale.h>
#include <qdatetime.h>
#include <qregexp.h>
#include <cups/cups.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_CUPS_NO_PWD_CACHE
#include <qcstring.h>
static QCString cups_authstring = "";
#endif

// CUPS used to keep octetString values in string.text; CUPS 2 keeps them as
// (data, length), which ippGetString() does not return. Read the value up to
// the first NUL byte, as the string.text access did, but within its length.
static QCString octetString(ipp_attribute_t *attr, int i)
{
	int	len = 0;
	const char	*data = (const char*)ippGetOctetString(attr, i, &len);
	return (data ? QCString(data, len+1) : QCString());
}

void dumpRequest(ipp_t *req, bool answer = false, const QString& s = QString::null)
{
	kdDebug(500) << "==========" << endl;
	if (s.isEmpty())
		kdDebug(500) << (answer ? "Answer" : "Request") << endl;
	else
		kdDebug(500) << s << endl;
	kdDebug(500) << "==========" << endl;
	if (!req)
	{
		kdDebug(500) << "Null request" << endl;
		return;
	}
	kdDebug(500) << "State = 0x" << QString::number(ippGetState(req), 16) << endl;
	kdDebug(500) << "ID = 0x" << QString::number(ippGetRequestId(req), 16) << endl;
	if (answer)
	{
		kdDebug(500) << "Status = 0x" << QString::number(ippGetStatusCode(req), 16) << endl;
		kdDebug(500) << "Status message = " << ippErrorString(ippGetStatusCode(req)) << endl;
	}
	else
		kdDebug(500) << "Operation = 0x" << QString::number(ippGetOperation(req), 16) << endl;
	int	minor;
	int	major = ippGetVersion(req, &minor);
	kdDebug(500) << "Version = " << major << "." << minor << endl;
	kdDebug(500) << endl;

	ipp_attribute_t *attr = ippFirstAttribute(req);
	while (attr)
	{
		QString s = QString::fromLatin1("%1 (0x%2) = ").arg(ippGetName(attr)).arg(ippGetValueTag(attr), 0, 16);
		for (int i=0;i<ippGetCount(attr);i++)
		{
			switch (ippGetValueTag(attr))
			{
				case IPP_TAG_INTEGER:
				case IPP_TAG_ENUM:
					s += ("0x"+QString::number(ippGetInteger(attr, i), 16));
					break;
				case IPP_TAG_BOOLEAN:
					s += (ippGetBoolean(attr, i) ? "true" : "false");
					break;
				case IPP_TAG_STRING:
					s += octetString(attr, i).data();
					break;
				case IPP_TAG_TEXT:
				case IPP_TAG_NAME:
				case IPP_TAG_KEYWORD:
				case IPP_TAG_URI:
				case IPP_TAG_MIMETYPE:
				case IPP_TAG_NAMELANG:
				case IPP_TAG_TEXTLANG:
				case IPP_TAG_CHARSET:
				case IPP_TAG_LANGUAGE:
					s += ippGetString(attr, i, NULL);
					break;
				default:
					break;
			}
			if (i != (ippGetCount(attr)-1))
				s += ", ";
		}
		kdDebug(500) << s << endl;
		attr = ippNextAttribute(req);
	}
}

QString errorString(int status)
{
	QString	str;
	switch (status)
	{
		case IPP_FORBIDDEN:
			str = i18n("You don't have access to the requested resource.");
			break;
		case IPP_NOT_AUTHORIZED:
			str = i18n("You are not authorized to access the requested resource.");
			break;
		case IPP_NOT_POSSIBLE:
			str = i18n("The requested operation cannot be completed.");
			break;
		case IPP_SERVICE_UNAVAILABLE:
			str = i18n("The requested service is currently unavailable.");
			break;
		case IPP_NOT_ACCEPTING:
			str = i18n("The target printer is not accepting print jobs.");
			break;
		default:
			str = QString::fromLocal8Bit(ippErrorString((ipp_status_t)status));
			break;
	}
	return str;
}

//*************************************************************************************

IppRequest::IppRequest()
{
	request_ = 0;
	port_ = -1;
	host_ = QString::null;
	dump_ = 0;
	init();
}

IppRequest::~IppRequest()
{
	ippDelete(request_);
}

void IppRequest::init()
{
	connect_ = true;

	if (request_)
	{
		ippDelete(request_);
		request_ = 0;
	}
	request_ = ippNew();
	//kdDebug(500) << "kdeprint: IPP request, lang=" << KGlobal::locale()->language() << endl;
        QCString langstr = KGlobal::locale()->language().latin1();
	cups_lang_t*	lang = cupsLangGet(langstr.data());
	// default charset to UTF-8 (ugly hack)
	lang->encoding = CUPS_UTF8;
	ippAddString(request_, IPP_TAG_OPERATION, IPP_TAG_CHARSET, "attributes-charset", NULL, cupsLangEncoding(lang));
	ippAddString(request_, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE, "attributes-natural-language", NULL, lang->language);
	cupsLangFree(lang);
}

void IppRequest::addString_p(int group, int type, const QString& name, const QString& value)
{
	if (!name.isEmpty())
		ippAddString(request_,(ipp_tag_t)group,(ipp_tag_t)type,name.latin1(),NULL,(value.isEmpty() ? "" : value.local8Bit().data()));
}

void IppRequest::addStringList_p(int group, int type, const QString& name, const QStringList& values)
{
	if (!name.isEmpty())
	{
		ipp_attribute_t	*attr = ippAddStrings(request_,(ipp_tag_t)group,(ipp_tag_t)type,name.latin1(),(int)(values.count()),NULL,NULL);
		int	i(0);
		for (QStringList::ConstIterator it=values.begin(); it != values.end(); ++it, i++)
			ippSetString(request_, &attr, i, (*it).local8Bit());
	}
}

void IppRequest::addInteger_p(int group, int type, const QString& name, int value)
{
	if (!name.isEmpty()) ippAddInteger(request_,(ipp_tag_t)group,(ipp_tag_t)type,name.latin1(),value);
}

void IppRequest::addIntegerList_p(int group, int type, const QString& name, const QValueList<int>& values)
{
	if (!name.isEmpty())
	{
		ipp_attribute_t	*attr = ippAddIntegers(request_,(ipp_tag_t)group,(ipp_tag_t)type,name.latin1(),(int)(values.count()),NULL);
		int	i(0);
		for (QValueList<int>::ConstIterator it=values.begin(); it != values.end(); ++it, i++)
			ippSetInteger(request_, &attr, i, *it);
	}
}

void IppRequest::addBoolean(int group, const QString& name, bool value)
{
	if (!name.isEmpty()) ippAddBoolean(request_,(ipp_tag_t)group,name.latin1(),(char)value);
}

void IppRequest::addBoolean(int group, const QString& name, const QValueList<bool>& values)
{
	if (!name.isEmpty())
	{
		ipp_attribute_t	*attr = ippAddBooleans(request_,(ipp_tag_t)group,name.latin1(),(int)(values.count()),NULL);
		int	i(0);
		for (QValueList<bool>::ConstIterator it=values.begin(); it != values.end(); ++it, i++)
			ippSetBoolean(request_, &attr, i, (char)(*it));
	}
}

void IppRequest::setOperation(int op)
{
	ippSetOperation(request_, (ipp_op_t)op);
	ippSetRequestId(request_, 1);	// 0 is not RFC-compliant, should be at least 1
}

int IppRequest::status()
{
	return (request_ ? ippGetStatusCode(request_) : (connect_ ? cupsLastError() : -2));
}

QString IppRequest::statusMessage()
{
	QString	msg;
	switch (status())
	{
		case -2:
			msg = i18n("Connection to CUPS server failed. Check that the CUPS server is correctly installed and running.");
			break;
		case -1:
			msg = i18n("The IPP request failed for an unknown reason.");
			break;
		default:
			msg = errorString(status());
			break;
	}
	return msg;
}

bool IppRequest::integerValue_p(const QString& name, int& value, int type)
{
	if (!request_ || name.isEmpty()) return false;
	ipp_attribute_t	*attr = ippFindAttribute(request_, name.latin1(), (ipp_tag_t)type);
	if (attr)
	{
		value = ippGetInteger(attr, 0);
		return true;
	}
	else return false;
}

bool IppRequest::stringValue_p(const QString& name, QString& value, int type)
{
	if (!request_ || name.isEmpty()) return false;
	ipp_attribute_t	*attr = ippFindAttribute(request_, name.latin1(), (ipp_tag_t)type);
	if (attr)
	{
		value = QString::fromLocal8Bit(ippGetString(attr, 0, NULL));
		return true;
	}
	else return false;
}

bool IppRequest::stringListValue_p(const QString& name, QStringList& values, int type)
{
	if (!request_ || name.isEmpty()) return false;
	ipp_attribute_t	*attr = ippFindAttribute(request_, name.latin1(), (ipp_tag_t)type);
	values.clear();
	if (attr)
	{
		for (int i=0;i<ippGetCount(attr);i++)
			values.append(QString::fromLocal8Bit(ippGetString(attr, i, NULL)));
		return true;
	}
	else return false;
}

bool IppRequest::boolean(const QString& name, bool& value)
{
	if (!request_ || name.isEmpty()) return false;
	ipp_attribute_t	*attr = ippFindAttribute(request_, name.latin1(), IPP_TAG_BOOLEAN);
	if (attr)
	{
		value = (bool)ippGetBoolean(attr, 0);
		return true;
	}
	else return false;
}

bool IppRequest::doFileRequest(const QString& res, const QString& filename)
{
	QString	myHost = host_;
	int 	myPort = port_;
	if (myHost.isEmpty()) myHost = CupsInfos::self()->host();
	if (myPort <= 0) myPort = CupsInfos::self()->port();
	http_t	*HTTP = httpConnect(myHost.latin1(),myPort);

	connect_ = (HTTP != NULL);

	if (HTTP == NULL)
	{
		ippDelete(request_);
		request_ = 0;
		return false;
	}

#ifdef HAVE_CUPS_NO_PWD_CACHE
#if CUPS_VERSION_MAJOR == 1 && CUPS_VERSION_MINOR <= 2
   strncpy(  HTTP->authstring, cups_authstring.data(), HTTP_MAX_VALUE );
#else
   httpSetAuthString( HTTP, NULL, cups_authstring.data() );
#endif
#endif

	if (dump_ > 0)
	{
		dumpRequest(request_, false, "Request to "+myHost+":"+QString::number(myPort));
	}

	request_ = cupsDoFileRequest(HTTP, request_, (res.isEmpty() ? "/" : res.latin1()), (filename.isEmpty() ? NULL : filename.latin1()));
#ifdef HAVE_CUPS_NO_PWD_CACHE
#if CUPS_VERSION_MAJOR == 1 && CUPS_VERSION_MINOR <= 2
   cups_authstring = HTTP->authstring;
#else
	cups_authstring = httpGetAuthString( HTTP );
#endif
#endif
	httpClose(HTTP);

	if (dump_ > 1)
	{
		dumpRequest(request_, true);
	}

	/* No printers found */
	if ( request_ && ippGetStatusCode(request_) == 0x406 )
		return true;

	if (!request_ || ippGetState(request_) == IPP_ERROR || (ippGetStatusCode(request_) & 0x0F00))
		return false;


	return true;
}

bool IppRequest::htmlReport(int group, QTextStream& output)
{
	if (!request_) return false;
	// start table
	output << "<table border=\"1\" cellspacing=\"0\" cellpadding=\"0\">" << endl;
	output << "<tr><th bgcolor=\"dark blue\"><font color=\"white\">" << i18n("Attribute") << "</font></th>" << endl;
	output << "<th bgcolor=\"dark blue\"><font color=\"white\">" << i18n("Values") << "</font></th></tr>" << endl;
	// go to the first attribute of the specified group
	ipp_attribute_t	*attr = ippFirstAttribute(request_);
	while (attr && ippGetGroupTag(attr) != group)
		attr = ippNextAttribute(request_);
	// print each attribute
	const ipp_uchar_t	*d;
	QCString	dateStr;
	QDateTime	dt;
	bool	bg(false);
	while (attr && ippGetGroupTag(attr) == group)
	{
		output << "  <tr bgcolor=\"" << (bg ? "#ffffd9" : "#ffffff") << "\">\n    <td><b>" << ippGetName(attr) << "</b></td>\n    <td>" << endl;
		bg = !bg;
		for (int i=0; i<ippGetCount(attr); i++)
		{
			int	lower, upper, xres, yres;
			ipp_res_t	units;
			switch (ippGetValueTag(attr))
			{
				case IPP_TAG_INTEGER:
					if (ippGetName(attr) && strstr(ippGetName(attr), "time"))
					{
						dt.setTime_t((unsigned int)(ippGetInteger(attr, i)));
						output << dt.toString();
					}
					else
						output << ippGetInteger(attr, i);
					break;
				case IPP_TAG_ENUM:
					output << "0x" << hex << ippGetInteger(attr, i) << dec;
					break;
				case IPP_TAG_BOOLEAN:
					output << (ippGetBoolean(attr, i) ? i18n("True") : i18n("False"));
					break;
				case IPP_TAG_STRING:
					output << octetString(attr, i);
					break;
				case IPP_TAG_TEXTLANG:
				case IPP_TAG_NAMELANG:
				case IPP_TAG_TEXT:
				case IPP_TAG_NAME:
				case IPP_TAG_KEYWORD:
				case IPP_TAG_URI:
				case IPP_TAG_CHARSET:
				case IPP_TAG_LANGUAGE:
				case IPP_TAG_MIMETYPE:
					output << ippGetString(attr, i, NULL);
					break;
				case IPP_TAG_RESOLUTION:
					xres = ippGetResolution(attr, i, &yres, &units);
					output << "( " << xres
					       << ", " << yres << " )";
					break;
				case IPP_TAG_RANGE:
					lower = ippGetRange(attr, i, &upper);
					output << "[ " << (lower > 0 ? lower : 1)
					       << ", " << (upper > 0 ? upper : 65535) << " ]";
					break;
				case IPP_TAG_DATE:
					d = ippGetDate(attr, i);
					dateStr.sprintf("%.4d-%.2d-%.2d, %.2d:%.2d:%.2d %c%.2d%.2d",
							d[0]*256+d[1], d[2], d[3],
							d[4], d[5], d[6],
							d[8], d[9], d[10]);
					output << dateStr;
					break;
				default:
					continue;
			}
			if (i < ippGetCount(attr)-1)
				output << "<br>";
		}
		output << "</td>\n  </tr>" << endl;
		attr = ippNextAttribute(request_);
	}
	// end table
	output << "</table>" << endl;

	return true;
}

QMap<QString,QString> IppRequest::toMap(int group)
{
	QMap<QString,QString>	opts;
	if (request_)
	{
		ipp_attribute_t	*attr = first();
		while (attr)
		{
			if (group != -1 && ippGetGroupTag(attr) != group)
			{
				attr = next();
				continue;
			}
			QString	value;
			for (int i=0; i<ippGetCount(attr); i++)
			{
				int	lower, upper;
				switch (ippGetValueTag(attr))
				{
					case IPP_TAG_INTEGER:
					case IPP_TAG_ENUM:
						value.append(QString::number(ippGetInteger(attr, i))).append(",");
						break;
					case IPP_TAG_BOOLEAN:
						value.append((ippGetBoolean(attr, i) ? "true" : "false")).append(",");
						break;
					case IPP_TAG_RANGE:
						lower = ippGetRange(attr, i, &upper);
						if (lower > 0)
							value.append(QString::number(lower));
						if (lower != upper)
						{
							value.append("-");
							if (upper > 0)
								value.append(QString::number(upper));
						}
						value.append(",");
						break;
					case IPP_TAG_STRING:
						value.append(QString::fromLocal8Bit(octetString(attr, i))).append(",");
						break;
					case IPP_TAG_TEXT:
					case IPP_TAG_NAME:
					case IPP_TAG_KEYWORD:
					case IPP_TAG_URI:
					case IPP_TAG_MIMETYPE:
					case IPP_TAG_NAMELANG:
					case IPP_TAG_TEXTLANG:
					case IPP_TAG_CHARSET:
					case IPP_TAG_LANGUAGE:
						value.append(QString::fromLocal8Bit(ippGetString(attr, i, NULL))).append(",");
						break;
					default:
						break;
				}
			}
			if (!value.isEmpty())
				value.truncate(value.length()-1);
			opts[QString::fromLocal8Bit(ippGetName(attr))] = value;
			attr = next();
		}
	}
	return opts;
}

void IppRequest::setMap(const QMap<QString,QString>& opts)
{
	if (!request_)
		return;

	QRegExp	re("^\"|\"$");
	cups_option_t	*options = NULL;
	int	n = 0;
	for (QMap<QString,QString>::ConstIterator it=opts.begin(); it!=opts.end(); ++it)
	{
		if (it.key().startsWith("kde-") || it.key().startsWith("app-"))
			continue;
		QString	value = it.data().stripWhiteSpace(), lovalue;
		value.replace(re, "");
		lovalue = value.lower();

		// handles specific cases: boolean, empty strings, or option that has that boolean
		// keyword as value (to prevent them from conversion to real boolean)
		if (value == "true" || value == "false")
			addBoolean(IPP_TAG_JOB, it.key(), (value == "true"));
		else if (value.isEmpty() || lovalue == "off" || lovalue == "on"
		         || lovalue == "yes" || lovalue == "no"
			 || lovalue == "true" || lovalue == "false")
			addName(IPP_TAG_JOB, it.key(), value);
		else
			n = cupsAddOption(it.key().local8Bit(), value.local8Bit(), n, &options);
	}
	if (n > 0)
		cupsEncodeOptions(request_, n, options);
	cupsFreeOptions(n, options);

	// find an remove that annoying "document-format" attribute
#if CUPS_VERSION_MAJOR == 1 && CUPS_VERSION_MINOR >= 2
    ipp_attribute_t *attr = ippFindAttribute(request_, "document-format", IPP_TAG_NAME);
    ippDeleteAttribute(request_, attr);
#else
	// (the first one that follows another attribute, whatever its value tag)
	ipp_attribute_t	*attr = ippFirstAttribute(request_);
	while (attr)
	{
		ipp_attribute_t	*attr2 = ippNextAttribute(request_);
		if (attr2 && ippGetName(attr2) && strcmp(ippGetName(attr2), "document-format") == 0)
		{
			ippDeleteAttribute(request_, attr2);
			break;
		}
		attr = attr2;
	}
#endif
}
