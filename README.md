Greeny is the Graphical, Really Easy Editor for torreNts, Yup!

## Dependencies

* C compiler
* MinGW (if you want to build for Windows)
* GTK Development libraries (if you want to build for Linux or Mac)

## Compiling

Run `make` to build for your own system, assuming you're on Linux or Mac (Autotools? You almost fooled me). To build for Windows, install the mingw cross compiler (or, ya know, just install it normally on Windows) and run the mingw make tool (often `mingw64-make` for the cross-compiler). Be sure to `make clean` between runs, otherwise you might end up trying to link Linux objects into a Windows executable!

On all platforms, dependencies are compiled statically, and no packaging or installers are needed.
