#!/usr/bin/env bash

grind() {
	echo "Testing 'greeny-cli $*'"
	valgrind --leak-check=full --error-exitcode=198 build/native/bin/greeny-cli "$@"
	(( $? == 198 )) && {
		echo 'Valgrind reported an error.';
		exit 1;
	}
}

# @param dir0
# @param dir1
# exits if not equal
assert_dir_eq() {
	diff -r "$1" "$2" || {
		echo "$1 and $2 were not equal.";
		exit 1;
	}
}

command -v valgrind >/dev/null || {
	echo 'Valgrind not found -- please install';
	exit 1;
}

grind
grind --orpheus opseus
grind --orpheus abcdef0123456789abcdef0123456789
grind --orpheus abcdef0123456789abcdef0123456789 build

mkdir -p .tmp

rm -rf .tmp/greeny-basic-in
cp -r tests/fixtures/basic-in .tmp/greeny-basic-in
grind --orpheus abcdef0123456789abcdef0123456789 .tmp/greeny-basic-in
assert_dir_eq .tmp/greeny-basic-in tests/fixtures/basic-out

echo
echo 'All tests passed.'
