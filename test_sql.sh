#!/bin/bash

# Simple test script; run after performing
# git clone https://github.com/mdbtools/mdbtestdata.git test
./src/util/mdb-sql -i test/sql/nwind.sql test/data/nwind.mdb

# Regression tests: run each tests/sql/*.sql against nwind.mdb and compare
# the output (stdout and stderr) with the matching .expected file.
# Run with --update to regenerate the .expected files.
rc=0
for sql in tests/sql/*.sql; do
	expected="${sql%.sql}.expected"
	actual="$(./src/util/mdb-sql -P -D '%Y-%m-%d' -T '%Y-%m-%d %H:%M:%S' \
		-i "$sql" test/data/nwind.mdb 2>&1)"
	if [ "$1" = "--update" ]; then
		printf '%s\n' "$actual" > "$expected"
	elif ! printf '%s\n' "$actual" | diff -u "$expected" -; then
		printf 'FAILED: %s\n' "$sql"
		rc=1
	fi
done
if [ $rc = 0 ]; then
	echo "SQL regression tests passed."
fi
exit $rc
