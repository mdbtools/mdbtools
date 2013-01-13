========================================================================
     mdbtools Project Overview
========================================================================

Author: The Jimmytaker (jimmytaker@gmx.de)

This is a port of the mdb-tools library to Windows. I adapted the mdb-tools 0.7 to work as a library in Windows as a part of my work that required me to develop a solution to port Access databases to SQLite usable under both Windows and OS X.

List of changes:

- Adapt the code to compile under both VS and MAC (declaration - initialization, void* - char*/unsigned char*, sprintf(), ssize_t, strcasecmp())
- Enforce consistency g_malloc - g_free and malloc - free
- Make reading 0 as date return "00:00:00" as  t->tm_year == -1 fails the debug assertion >= 0
- Introduce SQLite support.
- Allow schema generation to char* and not only to FILE* from within the library

Requirements to build and use:

The only dependency is on glib. You need to provide the glib headers for the project to be built.

