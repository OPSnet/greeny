#!/usr/bin/env bash

grind() {
	echo "Testing 'greeny-cli $*'"
	valgrind --leak-check=full --error-exitcode=198 build/native/bin/greeny-cli "$@"
	(( $? == 198 )) && {
		echo 'Valgrind reported an error.';
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

echo
echo 'All tests passed.'
