#!/bin/bash -e

if [ -z "$1" ]; then
    echo "Missing version"
    echo ""
    echo "Usage package.sh <version>"
    echo ""
    exit 1
fi

sudo checkinstall -y --pkgname=cyber-mdbtools --pkgversion=$1 --pkggroup=Tools --pkgsource=https://github.com/cyberemissary/mdbtools/archive/master.zip \
--maintainer=admin@cyberemissary.com --provides=mdbtools