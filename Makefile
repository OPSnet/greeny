CFLAGS := $(CFLAGS) -Icontrib -Wall --std=c99

headers_common := contrib/bencode.h src/libannouncebulk.h src/err.h src/vector.h
sources_common := contrib/bencode.o src/libannouncebulk.o src/vector.o

headers_cli    := $(headers_common)
sources_cli    := $(sources_common) src/cli.o
binary_cli     := ./greeny-cli
LIBS_cli       := 

headers_test   := $(headers_common)
sources_test   := $(sources_common) tests/test.c
binary_test    := ./test-greeny
LIBS_test      := -lcmocka

all: $(binary_cli)

test: $(binary_test)
	$(binary_test)

$(binary_cli) : $(sources_cli) $(headers_cli)
	$(CC) -o $(binary_cli) $(LIBS_cli) $(sources_cli)

$(binary_test) : $(sources_test) $(headers_test)
	$(CC) -o $(binary_test) $(LIBS_test) $(sources_test)

clean:
	rm -f */*.o $(binary_cli) $(binary_test)

.PHONY: all test clean
