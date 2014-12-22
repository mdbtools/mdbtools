#-------------------------------------------------
#
# Project created by QtCreator 2013-02-25T15:54:10
#
# Need to provide glib, intl and iconv to use
#
#-------------------------------------------------

QT       -= core gui

TEMPLATE = lib

DEFINES += HAVE_ICONV \
    UNICODE NO_NULL_DT DT_FMT_ISODATE BUILDING_LIBMDB
win32: DEFINES += USING_STATIC_LIBICONV

CONFIG( debug, debug|release ) {
    TARGET = mdbD
    DESTDIR = binD
    OBJECTS_DIR += debug
    MOC_DIR += ./GeneratedFiles/debug
    INCLUDEPATH += ./GeneratedFiles/Debug
    win32: LIBS += ../libmdb/glibD.lib ../libmdb/libiconvStaticD.lib ../libmdb/intl.lib
} else {
    TARGET = mdb
    DESTDIR = binR
    OBJECTS_DIR += release
    MOC_DIR += ./GeneratedFiles/release
    INCLUDEPATH += ./GeneratedFiles/Release
    win32: LIBS += ../libmdb/glib.lib ../libmdb/libiconvStatic.lib ../libmdb/intl.lib
}
macx: LIBS += ../libmdb/libglib-2.0.0.dylib ../libmdb/libiconv.2.dylib

INCLUDEPATH += .\
    ../../include/

macx: INCLUDEPATH += ../../../glib-2.0
win32: INCLUDEPATH += ../../../libiconv/include ../../../glib-2.28.8 ../../../glib-2.28.8/glib

HEADERS += ../../include/mdbtools.h

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

win32: LIBS += \
    #windows API libs
    -lkernel32 -luser32 -lgdi32 -lwinspool -lcomdlg32 -ladvapi32 -lshell32 -lole32 -oleaut32 -luuid -odbc32 -odbccp32
