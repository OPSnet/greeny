Greeny is the Graphical, Really Easy Editor for torreNts, Yup!

## Linux & Mac Dependencies

* GCC (clang will probably work too, but praise Stallman!)
* Make
* `unzip` utility (if you plan on using the automatic IUP download and extractor)
* GTK+ with development headers

## Windows Cross-Compilation Dependencies

* MinGW Cross-compiler
* MinGW Make
* MinGW libgnurx, static edition, with development headers. WARNING: Some distros, like openSUSE, ship a version of libgnurx that has "static" .a files, but those files actually aren't really static and still will require runtime linkage! I recommend downloading the Fedora version of libgnurx-static from pkgs.org and dumping the included `libregex.a` file into the correct spot in your filesystem. On openSUSE, you can actually just install the RPM and ignore dependency problems, it will "just work".

## Getting IUP

IUP is a cross-platform GUI framework used in Greeny. IUP is not commonly packaged and thus a custom method of acquisition becomes a requirement. Run `make download_iup` to automatically download IUP from sourceforge. If your spidey senses are tingling, be safe and manually download the sources to the vendor/iup folder. Greeny's Makefile will compile IUP as part of the GUI target.

## Compiling

`make` will build for your native Linux or Mac. Use the mingw cross compiler's built-in make tool (often `mingw64-make`) to build for Windows. Build artifacts and binaries will be put in a separate folder (build/windows), so you can switch between make and mingw64-make as often as you'd like. Binaries will be put in build/native/bin for Linux/Mac, and build/windows/bin for Windows.

Run `make clean_greeny_only` to clear greeny's artifacts or `make clean` to completely remove both greeny's build artifacts and vendor/iup (Note, however, that if you )
