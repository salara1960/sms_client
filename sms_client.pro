#-------------------------------------------------
#
# Project created by QtCreator 2016-05-28T17:07:01
#
#-------------------------------------------------

QMAKE_CXX += -std=gnu++11 -O2 -masm=att

QT += widgets core gui network sql

TARGET = sms_client
TEMPLATE = app

SOURCES += main.cpp\
        mainwindow.cpp

HEADERS  += mainwindow.h

FORMS    += mainwindow.ui\
        cmd_all.ui

RESOURCES += icons.qrc
