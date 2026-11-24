/*  This file is part of the KDE libraries
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License version 2 as published by the Free Software Foundation;
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

#ifndef KBALLOONTEST_H
#define KBALLOONTEST_H

#include <qobject.h>

class KBalloon;

// prints what the balloons do (for the scripts that click them)
class Reporter : public QObject
{
    Q_OBJECT

public:
    Reporter( QObject* parent = 0 );
    void watch( KBalloon* balloon, int number );
    virtual bool eventFilter( QObject* o, QEvent* e );

public slots:
    void report();
    void step(); // the next step of the scenario "queue"
    void moveStep(); // the scenario "move"
    void reentrantStep(); // the scenario "reentrant"

private slots:
    void clicked();
    void actionActivated( int id );
    void timedOut();
    void gone( QObject* balloon );

private:
    int number( const QObject* balloon ) const;
    void print( const QObject* balloon, const char* what, int id = -1 ) const;
};

#endif
