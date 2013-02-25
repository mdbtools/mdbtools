#-------------------------------------------------
#
# Project created by QtCreator 2013-02-25T15:54:10
#
#-------------------------------------------------

QT       -= core gui

TARGET = libmdb
TEMPLATE = lib
CONFIG += staticlib

DEFINES += HAVE_ICONV \
    UNICODE \

INCLUDEPATH += \
    ../../include/ \
    ../../../glib-2.0

SOURCES += \
    write.c \
    worktable.c \
    table.c \
    stats.c \
    sargs.c \
    props.c \
    options.c \
    money.c \
    mem.c \
    map.c \
    like.c \
    index.c \
    iconv.c \
    file.c \
    dump.c \
    data.c \
    catalog.c \
    backend.c

unix:!symbian {
    maemo5 {
        target.path = /opt/usr/lib
    } else {
        target.path = /usr/lib
    }
    INSTALLS += target
}
