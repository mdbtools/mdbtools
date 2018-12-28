#!/bin/bash -ex

function usage(){
    echo ""
    echo "This script packages mdbtools into a .deb and .rpm packages"
    echo ""
    echo "Usage package.sh <version> [<release>]"
    echo ""
    echo "Arguments:"
    echo "  <version> is the version to build"
    echo "  <release> is the release number to build; optional (default 1)"
    echo ""
}

if [ -z "$1" ]; then
    echo "ERROR: Missing version"
    usage
    exit 1
fi

if [ "--help" = "$1" ]; then
    usage
    exit 0
fi

PKG_VERSION=$1
PKG_RELEASE=1

if [ ! -z "$2" ]; then
    PKG_RELEASE=$2
fi

sudo checkinstall -y --pkgname=cyber-mdbtools --pkgversion=${PKG_VERSION} --pkgrelease=${PKG_RELEASE} --pkggroup=Tools \
--pkgsource=https://github.com/cyberemissary/mdbtools/archive/master.zip \
--maintainer=admin@cyberemissary.com --provides=mdbtools

# remove directory in case we are rebuilding the same version and we quite prematurely before
rm -Rf cyber-mdbtools-${PKG_VERSION}
sudo alien -r -g -v -k cyber-mdbtools_${PKG_VERSION}-${PKG_RELEASE}_amd64.deb

cd cyber-mdbtools-${PKG_VERSION}
sudo rpmbuild --buildroot $(pwd)/ --bb cyber-mdbtools-${PKG_VERSION}-${PKG_RELEASE}.spec
cd 