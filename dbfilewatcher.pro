QT -= gui
QT += sql

CONFIG += c++11 c++14 console
CONFIG -= app_bundle

# The following define makes your compiler emit warnings if you use
# any feature of Qt which as been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
        main.cpp \
    database.cpp \
    modifiedfilesystemwatcher.cpp \
    dbfilewatcher.cpp \
    utility.cpp

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

HEADERS += \
    bokzdbexceptions.h \
    database.h \
    utility.h \
    modifiedfilesystemwatcher.h \
    dbfilewatcher.h \
    utility.h

win32: LIBS += -L$$PWD/../../../../PostgreSQL/9.6/lib/ -llibpq

INCLUDEPATH += $$PWD/../../../../PostgreSQL/9.6/include
DEPENDPATH += $$PWD/../../../../PostgreSQL/9.6/include
