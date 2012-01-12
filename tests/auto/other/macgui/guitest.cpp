/****************************************************************************
**
** Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** GNU Lesser General Public License Usage
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this
** file. Please review the following information to ensure the GNU Lesser
** General Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU General
** Public License version 3.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this
** file. Please review the following information to ensure the GNU General
** Public License version 3.0 requirements will be met:
** http://www.gnu.org/copyleft/gpl.html.
**
** Other Usage
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
**
**
**
**
** $QT_END_LICENSE$
**
****************************************************************************/


#include "guitest.h"
#include <QDebug>
#include <QWidget>
#include <QStack>
#include <QTimer>
#include <QtTest/QtTest>

#ifdef Q_OS_MAC
#   include <ApplicationServices/ApplicationServices.h>
#endif


/*
    Not really a test, just prints interface info.
*/
class PrintTest : public TestBase
{
public:
    bool operator()(QAccessibleInterface *candidate)
    {
        qDebug() << "";
        qDebug() << "Name" << candidate->text(QAccessible::Name);
        qDebug() << "Pos" <<  candidate->rect();
        qDebug() << "Number of children" << candidate->childCount();
        return false;
    }
};

class NameTest : public TestBase
{
public:
    NameTest(const QString &text, QAccessible::Text textType) : text(text), textType(textType) {}
    QString text;
    QAccessible::Text textType;

    bool operator()(QAccessibleInterface *candidate)
    {
        return (candidate->text(textType) == text);
    }
};

void WidgetNavigator::printAll(QWidget *widget)
{
    QAccessibleInterface * const iface = QAccessible::queryAccessibleInterface(widget);
    deleteInDestructor(iface);
    printAll(iface);
}

void WidgetNavigator::printAll(QAccessibleInterface *interface)
{
    PrintTest printTest;
    recursiveSearch(&printTest, interface);
}

QAccessibleInterface *WidgetNavigator::find(QAccessible::Text textType, const QString &text, QWidget *start)
{
    QAccessibleInterface *const iface = QAccessible::queryAccessibleInterface(start);
    deleteInDestructor(iface);
    return find(textType, text, iface);
}

QAccessibleInterface *WidgetNavigator::find(QAccessible::Text textType, const QString &text, QAccessibleInterface *start)
{
    NameTest nameTest(text, textType);
    return recursiveSearch(&nameTest, start);
}

/*
    Recursiveley navigates the accessible hiearchy looking for an interface that
    passsed the Test (meaning it returns true).
*/
QAccessibleInterface *WidgetNavigator::recursiveSearch(TestBase *test, QAccessibleInterface *iface)
{
    QStack<QAccessibleInterface *> todoInterfaces;
    todoInterfaces.push(iface);

    while (todoInterfaces.isEmpty() == false) {
        QAccessibleInterface *testInterface = todoInterfaces.pop();
        
        if ((*test)(testInterface))
            return testInterface;
            
        const int numChildren = testInterface->childCount();
        for (int i = 0; i < numChildren; ++i) {
            QAccessibleInterface *childInterface = testInterface->child(i);
            if (childInterface) {
                todoInterfaces.push(childInterface);
                deleteInDestructor(childInterface);
            }
        }
    }
    return 0;
}

void WidgetNavigator::deleteInDestructor(QAccessibleInterface *interface)
{
    interfaces.insert(interface);
}

QWidget *WidgetNavigator::getWidget(QAccessibleInterface *interface)
{
    return qobject_cast<QWidget *>(interface->object());
}

WidgetNavigator::~WidgetNavigator()
{
    foreach(QAccessibleInterface *interface, interfaces) {
        delete interface;
    }
}

///////////////////////////////////////////////////////////////////////////////

namespace NativeEvents {
#ifdef Q_OS_MAC
   void mouseClick(const QPoint &globalPos, Qt::MouseButtons buttons, MousePosition updateMouse)
    {
        CGPoint position;
        position.x = globalPos.x();
        position.y = globalPos.y();
       
        const bool updateMousePosition = (updateMouse == UpdatePosition);
        
        // Mouse down.
        CGPostMouseEvent(position, updateMousePosition, 3, 
                        (buttons & Qt::LeftButton) ? true : false, 
                        (buttons & Qt::MidButton/* Middlebutton! */) ? true : false, 
                        (buttons & Qt::RightButton) ? true : false);

        // Mouse up.
        CGPostMouseEvent(position, updateMousePosition, 3, false, false, false);	
    }
#else
# error Oops, NativeEvents::mouseClick() is not implemented on this platform.
#endif
};

///////////////////////////////////////////////////////////////////////////////

GuiTester::GuiTester()
{
    clearSequence();
}

GuiTester::~GuiTester()
{
    foreach(DelayedAction *action, actions)
        delete action;
}

bool checkPixel(QColor pixel, QColor expected)
{
    const int allowedDiff = 20;

    return !(qAbs(pixel.red() - expected.red()) > allowedDiff ||
            qAbs(pixel.green() - expected.green()) > allowedDiff ||
            qAbs(pixel.blue() - expected.blue()) > allowedDiff);
}

/*
    Tests that the pixels inside rect in image all have the given color. 
*/
bool GuiTester::isFilled(const QImage image, const QRect &rect, const QColor &color)
{
    for (int y = rect.top(); y <= rect.bottom(); ++y)
        for (int x = rect.left(); x <= rect.right(); ++x) {
            const QColor pixel = image.pixel(x, y);
            if (checkPixel(pixel, color) == false) {
//                qDebug()<< "Wrong pixel value at" << x << y << pixel.red() << pixel.green() << pixel.blue();
                return false;
            }
        }
    return true;
}


/*
    Tests that stuff is painted to the pixels inside rect.
    This test fails if any lines in the given direction have pixels 
    of only one color.
*/
bool GuiTester::isContent(const QImage image, const QRect &rect, Directions directions)
{
    if (directions & Horizontal) {
        for (int y = rect.top(); y <= rect.bottom(); ++y) {
            QColor currentColor = image.pixel(rect.left(), y);
            bool fullRun = true;
            for (int x = rect.left() + 1; x <= rect.right(); ++x) {
                if (checkPixel(image.pixel(x, y), currentColor) == false) {
                    fullRun = false;
                    break;
                }
            }
            if (fullRun) {
//                qDebug() << "Single-color line at horizontal line " << y  << currentColor;
                return false;
            }
        }
        return true;
    } 

    if (directions & Vertical) {
       for (int x = rect.left(); x <= rect.right(); ++x) {
            QRgb currentColor = image.pixel(x, rect.top());
            bool fullRun = true;
            for (int y = rect.top() + 1; y <= rect.bottom(); ++y) {
                if (checkPixel(image.pixel(x, y), currentColor) == false) {
                    fullRun = false;
                    break;
                }
            }
            if (fullRun) {
//                qDebug() << "Single-color line at vertical line" << x << currentColor;
                return false;
            }
        }
        return true;
    }
    return false; // shut the compiler up.
}

void DelayedAction::run()
{
    if (next)
        QTimer::singleShot(next->delay, next, SLOT(run()));
};

/*
    Schedules a mouse click at an interface using a singleShot timer.
    Only one click can be scheduled at a time.
*/
ClickLaterAction::ClickLaterAction(QAccessibleInterface *interface, Qt::MouseButtons buttons)
{
    this->useInterface = true;
    this->interface = interface;
    this->buttons = buttons;
}

/*
    Schedules a mouse click at a widget using a singleShot timer.
    Only one click can be scheduled at a time.
*/
ClickLaterAction::ClickLaterAction(QWidget *widget, Qt::MouseButtons buttons)
{
    this->useInterface = false;
    this->widget  = widget;
    this->buttons = buttons;
}

void ClickLaterAction::run()
{
    if (useInterface) {
        const QPoint globalCenter = interface->rect().center();
        NativeEvents::mouseClick(globalCenter, buttons);
    } else { // use widget
        const QSize halfSize = widget->size() / 2;
        const QPoint globalCenter = widget->mapToGlobal(QPoint(halfSize.width(), halfSize.height()));
        NativeEvents::mouseClick(globalCenter, buttons);
    }
    DelayedAction::run();
}

void GuiTester::clickLater(QAccessibleInterface *interface, Qt::MouseButtons buttons, int delay)
{
    clearSequence();
    addToSequence(new ClickLaterAction(interface, buttons), delay);
    runSequence();
}

void GuiTester::clickLater(QWidget *widget, Qt::MouseButtons buttons, int delay)
{
    clearSequence();
    addToSequence(new ClickLaterAction(widget, buttons), delay);
    runSequence();
}

void GuiTester::clearSequence()
{
    startAction = new DelayedAction();
    actions.insert(startAction);
    lastAction = startAction;
}

void GuiTester::addToSequence(DelayedAction *action, int delay)
{
    actions.insert(action);
    action->delay = delay;
    lastAction->next = action;
    lastAction = action;
}

void GuiTester::runSequence()
{
    QTimer::singleShot(0, startAction, SLOT(run()));
}

void GuiTester::exitLoopSlot()
{
    QTestEventLoop::instance().exitLoop();
}

