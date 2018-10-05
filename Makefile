CFLAGS := $(CFLAGS) -Icontrib -Isrc -Wall --std=c99
ifdef GRN_DEBUG
	CFLAGS := $(CFLAGS) -g
endif

headers_common := contrib/bencode.h src/libannouncebulk.h src/err.h src/vector.h
sources_common := contrib/bencode.o src/libannouncebulk.o src/vector.o

headers_cli    := $(headers_common)
sources_cli    := $(sources_common) src/cli.o
binary_cli     := greeny-cli
LIBS_cli       := 

all: $(binary_cli)

$(binary_cli) : $(sources_cli) $(headers_cli)
	$(CC) -o $(binary_cli) $(LIBS_cli) $(sources_cli)

clean:
	rm -f */*.o $(binary_cli)

.PHONY: clean
