ifneq ($(findstring mingw,$(CC)),)
	windows    := true
	bin_dir    := build/windows/bin
	obj_dir    := build/windows/obj
else
	bin_dir    := build/native/bin
	obj_dir    := build/native/obj
endif

CFLAGS := $(CFLAGS) -Icontrib -Wall --std=c99

LIBS_test      := -lcmocka
ifdef windows
	binary_suffix := .exe

	LIBS_cli      := -l:libgnurx.dll.a
else
	LIBS_cli      := 
endif

objs_common    := $(obj_dir)/bencode.o $(obj_dir)/libannouncebulk.o $(obj_dir)/vector.o

objs_cli       := $(objs_common) $(obj_dir)/cli.o
binary_cli     := $(bin_dir)/greeny-cli$(binary_suffix)

objs_test      := $(objs_common) $(obj_dir)/test.o
binary_test    := $(bin_dir)/test-greeny$(binary_suffix)

all: $(binary_cli)

test: $(binary_test)
	$(binary_test)

$(binary_cli) : $(objs_cli)
	$(CC) -o $(binary_cli) $(objs_cli) $(LIBS_cli)

$(binary_test) : $(objs_test)
	$(CC) -o $(binary_test) $(objs_test) $(LIBS_test)

$(obj_dir)/%.o : */%.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f */*.o $(binary_cli) $(binary_test)

.PHONY: all test clean
