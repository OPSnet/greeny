Greeny is the Graphical, Really Easy Editor for torreNts, Yup!

## Linux & Mac Dependencies

* GCC (clang will probably work too, but praise Stallman!)
* Make
* GTK+ with development headers

## Windows Cross-Compilation Dependencies

* MinGW Cross-compiler
* MinGW Make
* MinGW libgnurx with development headers (this is because MinGW does not ship with the POSIX regular expression libraries).

## Compiling

`make` will build for your native Linux or Mac. Use the mingw cross compiler's built-in make tool (often `mingw64-make`) to build for Windows. Build artifacts and binaries will be put in a separate folder (build/windows), so you can switch between make and mingw64-make as often as you'd like. Binaries will be put in build/native/bin for Linux/Mac, and build/windows/bin for Windows.
