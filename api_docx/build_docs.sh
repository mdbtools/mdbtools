#!/bin/bash

set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
echo "DIR = $DIR"

ROOT="$(dirname "$DIR")"
echo "ROOT = $ROOT"

echo "cleanup $ROOT/temp-man-pages"
rm -f -r $ROOT/temp-man-pages

mkdir $ROOT/temp-man-pages

echo "create man pages"
python $ROOT/api_docx/pre_build.py

doxygen $ROOT/api_docx/doxygen.conf