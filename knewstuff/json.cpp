/*
    This file is part of KNewStuff.
    Copyright (c) 2026 The KDE 3 revival authors

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "json.h"

#include <math.h>

#include <qmap.h>
#include <qvaluelist.h>

using namespace KNS;

static const int maxJsonDepth = 32;
static const int maxJsonValues = 200000;

namespace {

class JsonReader
{
  public:
    JsonReader( const QByteArray &data )
      : p( reinterpret_cast<const uchar *>( data.data() ) ), n( data.size() ),
        pos( 0 ), values( 0 ) {}

    bool parse( QVariant *result, QString *error )
    {
      // a UTF-8 byte order mark is tolerated
      if ( n >= 3 && p[ 0 ] == 0xef && p[ 1 ] == 0xbb && p[ 2 ] == 0xbf )
        pos = 3;
      bool ok = value( result, 0 );
      if ( ok ) {
        skipSpace();
        if ( pos != n ) {
          ok = false;
          message = "trailing data";
        }
      }
      if ( !ok && error )
        *error = QString( "%1 at byte %2" ).arg( message ).arg( pos );
      return ok;
    }

  private:
    void skipSpace()
    {
      while ( pos < n && ( p[ pos ] == ' ' || p[ pos ] == '\t' || p[ pos ] == '\n' || p[ pos ] == '\r' ) )
        ++pos;
    }

    bool fail( const char *m )
    {
      message = m;
      return false;
    }

    bool literal( const char *word )
    {
      uint len = qstrlen( word );
      if ( pos + len > n || qstrncmp( reinterpret_cast<const char *>( p + pos ), word, len ) != 0 )
        return fail( "invalid literal" );
      pos += len;
      return true;
    }

    bool value( QVariant *v, int depth )
    {
      if ( ++values > maxJsonValues )
        return fail( "too many values" );
      skipSpace();
      if ( pos >= n )
        return fail( "unexpected end" );
      switch ( p[ pos ] ) {
        case '{':
          return object( v, depth + 1 );
        case '[':
          return array( v, depth + 1 );
        case '"': {
          QString s;
          if ( !string( &s ) )
            return false;
          *v = QVariant( s );
          return true;
        }
        case 't':
          *v = QVariant( true, 0 );
          return literal( "true" );
        case 'f':
          *v = QVariant( false, 0 );
          return literal( "false" );
        case 'n':
          *v = QVariant();
          return literal( "null" );
        default:
          return number( v );
      }
    }

    bool object( QVariant *v, int depth )
    {
      if ( depth > maxJsonDepth )
        return fail( "nested too deeply" );
      ++pos;                              // '{'
      QMap<QString, QVariant> map;
      skipSpace();
      if ( pos < n && p[ pos ] == '}' ) {
        ++pos;
        *v = QVariant( map );
        return true;
      }
      for ( ;; ) {
        skipSpace();
        if ( pos >= n || p[ pos ] != '"' )
          return fail( "expected a key" );
        QString key;
        if ( !string( &key ) )
          return false;
        skipSpace();
        if ( pos >= n || p[ pos ] != ':' )
          return fail( "expected ':'" );
        ++pos;
        QVariant member;
        if ( !value( &member, depth ) )
          return false;
        if ( !map.contains( key ) )
          map.insert( key, member );
        skipSpace();
        if ( pos >= n )
          return fail( "unexpected end" );
        if ( p[ pos ] == ',' ) {
          ++pos;
          continue;
        }
        if ( p[ pos ] == '}' ) {
          ++pos;
          break;
        }
        return fail( "expected ',' or '}'" );
      }
      *v = QVariant( map );
      return true;
    }

    bool array( QVariant *v, int depth )
    {
      if ( depth > maxJsonDepth )
        return fail( "nested too deeply" );
      ++pos;                              // '['
      QValueList<QVariant> list;
      skipSpace();
      if ( pos < n && p[ pos ] == ']' ) {
        ++pos;
        *v = QVariant( list );
        return true;
      }
      for ( ;; ) {
        QVariant element;
        if ( !value( &element, depth ) )
          return false;
        list.append( element );
        skipSpace();
        if ( pos >= n )
          return fail( "unexpected end" );
        if ( p[ pos ] == ',' ) {
          ++pos;
          continue;
        }
        if ( p[ pos ] == ']' ) {
          ++pos;
          break;
        }
        return fail( "expected ',' or ']'" );
      }
      *v = QVariant( list );
      return true;
    }

    int hex4()
    {
      if ( pos + 4 > n )
        return -1;
      int u = 0;
      for ( int i = 0; i < 4; ++i ) {
        uchar c = p[ pos + i ];
        u <<= 4;
        if ( c >= '0' && c <= '9' ) u |= c - '0';
        else if ( c >= 'a' && c <= 'f' ) u |= c - 'a' + 10;
        else if ( c >= 'A' && c <= 'F' ) u |= c - 'A' + 10;
        else return -1;
      }
      pos += 4;
      return u;
    }

    bool string( QString *s )
    {
      ++pos;                              // '"'
      uint run = pos;                     // start of the raw UTF-8 bytes
      for ( ;; ) {
        if ( pos >= n )
          return fail( "unterminated string" );
        uchar c = p[ pos ];
        if ( c == '"' || c == '\\' ) {
          if ( pos > run )
            *s += QString::fromUtf8( reinterpret_cast<const char *>( p + run ), pos - run );
          ++pos;
          if ( c == '"' )
            return true;
          if ( pos >= n )
            return fail( "unterminated string" );
          uchar e = p[ pos++ ];
          switch ( e ) {
            case '"': *s += '"'; break;
            case '\\': *s += '\\'; break;
            case '/': *s += '/'; break;
            case 'b': *s += '\b'; break;
            case 'f': *s += '\f'; break;
            case 'n': *s += '\n'; break;
            case 'r': *s += '\r'; break;
            case 't': *s += '\t'; break;
            case 'u': {
              int u = hex4();
              if ( u < 0 )
                return fail( "invalid \\u escape" );
              if ( u >= 0xd800 && u < 0xdc00 ) {
                // a high surrogate: valid only with a low one
                if ( pos + 6 <= n && p[ pos ] == '\\' && p[ pos + 1 ] == 'u' ) {
                  uint save = pos;
                  pos += 2;
                  int low = hex4();
                  if ( low >= 0xdc00 && low < 0xe000 ) {
                    *s += QChar( ushort( u ) );
                    *s += QChar( ushort( low ) );
                    break;
                  }
                  pos = save;
                }
                *s += QChar( ushort( 0xfffd ) );
              } else if ( u >= 0xdc00 && u < 0xe000 ) {
                *s += QChar( ushort( 0xfffd ) );
              } else {
                *s += QChar( ushort( u ) );
              }
              break;
            }
            default:
              return fail( "invalid escape" );
          }
          run = pos;
          continue;
        }
        if ( c < 0x20 )
          return fail( "control character in a string" );
        ++pos;
      }
    }

    bool number( QVariant *v )
    {
      uint start = pos;
      bool negative = false;
      if ( pos < n && p[ pos ] == '-' ) {
        negative = true;
        ++pos;
      }
      if ( pos >= n || p[ pos ] < '0' || p[ pos ] > '9' )
        return fail( "invalid value" );
      double d = 0;
      if ( p[ pos ] == '0' ) {
        ++pos;
      } else {
        while ( pos < n && p[ pos ] >= '0' && p[ pos ] <= '9' )
          d = d * 10 + ( p[ pos++ ] - '0' );
      }
      if ( pos < n && p[ pos ] == '.' ) {
        ++pos;
        if ( pos >= n || p[ pos ] < '0' || p[ pos ] > '9' )
          return fail( "invalid number" );
        double scale = 0.1;
        while ( pos < n && p[ pos ] >= '0' && p[ pos ] <= '9' ) {
          d += ( p[ pos++ ] - '0' ) * scale;
          scale /= 10;
        }
      }
      if ( pos < n && ( p[ pos ] == 'e' || p[ pos ] == 'E' ) ) {
        ++pos;
        bool negativeExp = false;
        if ( pos < n && ( p[ pos ] == '+' || p[ pos ] == '-' ) )
          negativeExp = p[ pos++ ] == '-';
        if ( pos >= n || p[ pos ] < '0' || p[ pos ] > '9' )
          return fail( "invalid number" );
        int exp = 0;
        while ( pos < n && p[ pos ] >= '0' && p[ pos ] <= '9' ) {
          if ( exp < 10000 )
            exp = exp * 10 + ( p[ pos ] - '0' );
          ++pos;
        }
        d *= pow( 10.0, negativeExp ? -exp : exp );
      }
      if ( pos - start > 400 )
        return fail( "number too long" );
      *v = QVariant( negative ? -d : d );
      return true;
    }

    const uchar *p;
    uint n;
    uint pos;
    int values;
    const char *message;
};

}

bool KNS::parseJson( const QByteArray &data, QVariant *result, QString *error )
{
  JsonReader reader( data );
  QVariant v;
  if ( !reader.parse( &v, error ) )
    return false;
  *result = v;
  return true;
}

QVariant KNS::jsonValue( const QVariant &object, const char *key )
{
  if ( object.type() != QVariant::Map )
    return QVariant();
  const QMap<QString, QVariant> map = object.toMap();
  QMap<QString, QVariant>::ConstIterator it = map.find( QString::fromLatin1( key ) );
  return it == map.end() ? QVariant() : it.data();
}

QString KNS::jsonString( const QVariant &object, const char *key )
{
  QVariant v = jsonValue( object, key );
  if ( v.type() == QVariant::String )
    return v.toString();
  if ( v.type() == QVariant::Double ) {
    double d = v.toDouble();
    if ( d == floor( d ) && fabs( d ) < 1e15 )
      return QString::number( Q_LLONG( d ) );
    return QString::number( d );
  }
  return QString::null;
}

// Numbers are bounded so that a conversion to int is always defined.
static double boundedNumber( double d, double def )
{
  if ( d != d )                         // NaN
    return def;
  if ( d > 2e9 )
    return 2e9;
  if ( d < -2e9 )
    return -2e9;
  return d;
}

double KNS::jsonNumber( const QVariant &object, const char *key, double def )
{
  QVariant v = jsonValue( object, key );
  if ( v.type() == QVariant::Double )
    return boundedNumber( v.toDouble(), def );
  if ( v.type() == QVariant::String ) {
    bool ok = false;
    double d = v.toString().left( 50 ).toDouble( &ok );
    if ( ok )
      return boundedNumber( d, def );
  }
  return def;
}

QValueList<QVariant> KNS::jsonList( const QVariant &object, const char *key )
{
  QVariant v = jsonValue( object, key );
  if ( v.type() == QVariant::List )
    return v.toList();
  return QValueList<QVariant>();
}

// ------------------------------------------------------------------

static QString decodeEntity( const QString &name )
{
  static const struct { const char *name; ushort code; } entities[] = {
    { "amp", '&' }, { "lt", '<' }, { "gt", '>' }, { "quot", '"' },
    { "apos", '\'' }, { "nbsp", 0xa0 }, { "copy", 0xa9 }, { "reg", 0xae },
    { "ndash", 0x2013 }, { "mdash", 0x2014 }, { "hellip", 0x2026 },
    { "lsquo", 0x2018 }, { "rsquo", 0x2019 }, { "ldquo", 0x201c },
    { "rdquo", 0x201d }, { "middot", 0xb7 }, { "deg", 0xb0 }, { 0, 0 }
  };
  if ( name.startsWith( "#" ) ) {
    bool ok = false;
    uint code = ( name.length() > 1 && ( name[ 1 ] == 'x' || name[ 1 ] == 'X' ) )
                ? name.mid( 2 ).toUInt( &ok, 16 ) : name.mid( 1 ).toUInt( &ok, 10 );
    if ( !ok || code == 0 || ( code >= 0xd800 && code < 0xe000 ) || code > 0x10ffff )
      return QString( QChar( ushort( 0xfffd ) ) );
    if ( code < 0x10000 )
      return QString( QChar( ushort( code ) ) );
    code -= 0x10000;
    QString s;
    s += QChar( ushort( 0xd800 + ( code >> 10 ) ) );
    s += QChar( ushort( 0xdc00 + ( code & 0x3ff ) ) );
    return s;
  }
  for ( int i = 0; entities[ i ].name; ++i )
    if ( name == entities[ i ].name )
      return QString( QChar( entities[ i ].code ) );
  return QString::null;
}

QString KNS::htmlToPlainText( const QString &html, uint max )
{
  // a bound on the work, whatever the server sends
  const QString in = html.left( 64 * 1024 );
  const uint n = in.length();
  QString out;
  uint i = 0;
  while ( i < n && out.length() < max + 64 ) {
    QChar c = in[ i ];
    if ( c == '<' ) {
      // a tag: skipped, quoted attribute values may contain '>'
      uint j = i + 1;
      QChar quote;
      while ( j < n ) {
        QChar t = in[ j ];
        if ( !quote.isNull() ) {
          if ( t == quote ) quote = QChar();
        } else if ( t == '"' || t == '\'' ) {
          quote = t;
        } else if ( t == '>' ) {
          break;
        }
        ++j;
      }
      QString tag = in.mid( i + 1, QMIN( j - i - 1, 10u ) ).lower();
      // the text of scripts and styles is no text
      if ( tag.startsWith( "script" ) || tag.startsWith( "style" ) ) {
        QString end = tag.startsWith( "script" ) ? "</script" : "</style";
        int k = in.find( end, j, false );
        j = k < 0 ? n : uint( k );
        while ( j < n && in[ j ] != '>' )
          ++j;
      }
      if ( tag.startsWith( "br" ) || tag.startsWith( "/p" ) || tag.startsWith( "p" ) ||
           tag.startsWith( "/div" ) || tag.startsWith( "/li" ) || tag.startsWith( "td" ) )
        out += ' ';
      i = j + 1;
      continue;
    }
    if ( c == '&' ) {
      int semi = in.find( ';', i + 1 );
      if ( semi > int( i ) && semi - int( i ) <= 10 ) {
        QString decoded = decodeEntity( in.mid( i + 1, semi - i - 1 ) );
        if ( !decoded.isNull() ) {
          out += decoded;
          i = semi + 1;
          continue;
        }
      }
    }
    out += c;
    ++i;
  }

  for ( uint k = 0; k < out.length(); ++k ) {
    ushort u = out[ k ].unicode();
    if ( u < 0x20 || u == 0x7f || u == 0xa0 || ( u >= 0x80 && u < 0xa0 ) ||
         u == 0x2028 || u == 0x2029 || ( u >= 0x202a && u <= 0x202e ) ||
         ( u >= 0x2066 && u <= 0x2069 ) )
      out[ k ] = ' ';
  }
  out = out.simplifyWhiteSpace();
  if ( out.length() > max )
    out = out.left( max );
  return out;
}
