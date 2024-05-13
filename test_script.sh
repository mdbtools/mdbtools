#!/bin/sh

# Simple test script; run after performing
# git clone https://github.com/mdbtools/mdbtestdata.git test

set -o errexit
set -o nounset

LANG=C.UTF-8
CDPATH= cd -- "$(dirname -- "$0")"

parseArgs() {
	MT_OUTPUT_KIND=verbose
	while :; do
		if test $# -lt 1; then
			break
		fi
		case "$1" in
			-q | --quiet)
				MT_OUTPUT_KIND=quiet
				;;
			-g | --github)
				MT_OUTPUT_KIND=github
				;;
			-h | --help)
				printf 'Syntax:\n%s [-q|--quiet|-g|--github] [-h|--help]\n' "$0"
				exit 0
				;;
			*)
				printf 'Unrecognized option: "%s"\n' "$1"
				exit 1
				;;
		esac
		shift
	done
}

testCommand() {
	testCommand_name="$1"
	shift
	case $MT_OUTPUT_KIND in
		verbose)
			printf '# Running %s (%s)\n' "$testCommand_name" "$*"
			if "./src/util/$testCommand_name" "$@"; then
				return 0
			fi
			return 1
			;;
		quiet)
			printf 'Testing %s (%s)... ' "$testCommand_name" "$*"
			if "./src/util/$testCommand_name" "$@" >/dev/null; then
				printf 'passed.\n'
				return 0
			fi
			return 1
			;;
		github)
			testCommand_tempFile="$(mktemp)"
			printf 'Testing %s (%s)... ' "$testCommand_name" "$*"
			if "./src/util/$testCommand_name" "$@" 2>&1 >"$testCommand_tempFile"; then
				printf 'passed.\n'
				testCommand_rc=0
			else
				printf 'failed.\n'
				testCommand_rc=1
			fi
			echo '::group::Output'
			cat "$testCommand_tempFile"
			echo '::endgroup::'
			unlink "$testCommand_tempFile"
			return $testCommand_rc
			;;
		*)
			printf 'Unrecognized MT_OUTPUT_KIND (%s)\n' "$MT_OUTPUT_KIND"
			exit 1
			;;
	esac
}

parseArgs "$@"

rc=0
if ! testCommand mdb-json test/data/ASampleDatabase.accdb "Asset Items"; then
	rc=1
fi
if ! testCommand mdb-json test/data/nwind.mdb "Umsätze"; then
	rc=1
fi
if ! testCommand mdb-count test/data/ASampleDatabase.accdb "Asset Items"; then
	rc=1
fi
if ! testCommand mdb-count test/data/nwind.mdb "Umsätze"; then
	rc=1
fi
if ! testCommand mdb-prop test/data/ASampleDatabase.accdb "Asset Items"; then
	rc=1
fi
if ! testCommand mdb-prop test/data/nwind.mdb "Umsätze"; then
	rc=1
fi
if ! testCommand mdb-schema test/data/ASampleDatabase.accdb; then
	rc=1
fi
if ! testCommand mdb-schema test/data/nwind.mdb; then
	rc=1
fi
if ! testCommand mdb-schema test/data/nwind.mdb -T "Umsätze" postgres; then
	rc=1
fi
if ! testCommand mdb-tables test/data/ASampleDatabase.accdb; then
	rc=1
fi
if ! testCommand mdb-tables test/data/nwind.mdb; then
	rc=1
fi
if ! testCommand mdb-ver test/data/ASampleDatabase.accdb; then
	rc=1
fi
if ! testCommand mdb-ver test/data/nwind.mdb; then
	rc=1
fi
if ! testCommand mdb-queries test/data/ASampleDatabase.accdb qryCostsSummedByOwner; then
	rc=1
fi

if [ $rc = 0 ]; then
	printf -- '\n%s passed.\n' "$0"
else
	printf -- '\n%s failed!\n' "$0"
fi
exit $rc
