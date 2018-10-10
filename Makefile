ifneq ($(findstring mingw,$(CC)),)
	windows    := true
endif

### BUILD DIRECTORIES
ifdef windows
	build_dir  := build/windows
else
	build_dir  := build/native
endif
bin_dir        := $(build_dir)/bin
obj_dir        := $(build_dir)/obj

### SOURCE OBJECTS
objs_common    := $(obj_dir)/bencode.o $(obj_dir)/libannouncebulk.o $(obj_dir)/vector.o
objs_cli       := $(objs_common) $(obj_dir)/cli.o
objs_gui       := $(objs_common) $(obj_dir)/gui.o
objs_test      := $(objs_common) $(obj_dir)/test.o

### BINARIES
ifdef windows
	binary_suffix := .exe
endif
binary_cli     := $(bin_dir)/greeny-cli$(binary_suffix)
binary_gui     := $(bin_dir)/greeny$(binary_suffix)
binary_test    := $(bin_dir)/greeny-test$(binary_suffix)

### IUP
iup_dir        ?= vendor/iup
# deferred -> immediate
iup_dir        := $(iup_dir)
iup_makedir    := $(iup_dir)/src
ifdef windows
	iup_a  := $(iup_dir)/lib/mingw6_64/libiup.a
else
	# TODO: make this flexible
	iup_a  := $(iup_dir)/lib/Linux418_64/libiup.a
endif

### IUP DOWNLOAD
iup_zip_tmp    := vendor/iup.zip
iup_zip_url    := https://sourceforge.net/projects/iup/files/latest/download
iup_extract_d  := vendor

### LIBS
ifdef windows
	LIBS_cli       := -l:libgnurx.dll.a
	LIBS_gui       := $(iup_a) -lgdi32 -lcomdlg32 -lcomctl32 -luuid -loleaut32 -lole32
else
	LIBS_cli       := 
	LIBS_gui       := $(iup_a) $(shell pkg-config --libs gtk+-3.0) -lX11
endif
LIBS_test              := -lcmocka

### CFLAGS
CFLAGS         := $(CFLAGS) -Icontrib -Wall --std=c99

all: $(binary_cli)

test: $(binary_test)
	$(binary_test)

$(binary_cli) : $(objs_cli)
	$(CC) -o $(binary_cli) $(objs_cli) $(LIBS_cli)

$(binary_test) : $(objs_test)
	$(CC) -o $(binary_test) $(objs_test) $(LIBS_test)

$(obj_dir)/%.o : */%.c
	$(CC) $(CFLAGS) -c -o $@ $<

ifdef windows
$(iup_a) : export OS        := Windows_NT
$(iup_a) : export TEC_UNAME := mingw6_64
$(iup_a) : export MINGW6_64 := $(prefix)
# specifying variables on the command line like so prevents them from being overridden.
# IUP isn't really designed to be cross-compiled.
$(iup_a) :
	$(MAKE) -C $(iup_makedir) CC=$(CC) LIBC=$(AR) RANLIB=$(RANLIB)
else
$(iup_a) :
	$(MAKE) -C $(iup_makedir)
endif

download_iup: $(iup_zip_tmp)
	rm -rf $(iup_dir)
	unzip $(iup_zip_tmp) -d $(iup_extract_d)
	rm -f $(iup_zip_tmp)

$(iup_zip_tmp) :
	curl -Lo $(iup_zip_tmp) $(iup_zip_url)

clean_greeny_only:
	rm -f $(obj_dir)/*.o $(binary_cli) $(binary_gui) $(binary_test)

clean:
	$(MAKE) clean
	rm -rf $(iup_dir) $(iup_zip_tmp)

.PHONY: all test download_iup clean_greeny_only clean
