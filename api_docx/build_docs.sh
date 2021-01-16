#!/bin/bash


DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
ROOT="$(dirname "$DIR")"

echo "cleanup"
rm -f -r $ROOT/temp-man-pages

mkdir $ROOT/temp-man-pages

echo "create man pages"
python $ROOT/api_docx/pre_build.py

doxygen $ROOT/api_docx/doxygen.conf
