* Try to build with Platform SDK's Windows 2000 build environment for manual
  NMake invokation.

* I try to avoid asm for the sake of CPU compatibility.

## zlib

* Use the `Makefile.msc` and use the DLLs. I manually make `include` and `lib`
  in `c:/zlib`. Docs say what files to copy for dynamic (`zdll`).

## OpenSSL

* Use ActiveState Perl, Strawberry includes a GCC that will foul MSVC/CMake.

* An EP curve file needs patching, just init a value (because -Werror). Builds
  fine otherwise.

* `zlib` instead of `zlib_dynamic, `--with-zlib-lib` points to `zdll.lib`.
  Include path is obvious.

* OpenSSL prefix should be installed to `c:/openssl` since CMake looks there.

* I think I enabled engines, but it doesn't matter

## libssh2

* I used NMake generator for this.

* Shared library.

* `_WINDLL` defined or it'll foul exports, not generating a `.lib`.

* `vsnprintf` is weird, use same shim for `vscprintf` and prepare to fix refs,
  like in `misc.c`.

## libgit2

Patchset trying to be cleaned up and upstreamed, but notices for builders:

* VC++6 generator (NMake probably fine?)
* No static CRT
* No Clar (involves Python)
* Build shared libraries
* Use SSH, with additional variables because autodetect is dumb
  * `LIBSSH2_FOUND` `BOOL:YES`
  * `LIBSSH2_INCLUDE_DIRS` `FILEPATH:C:/libssh2/include`
  * `LIBSSH2_LIBRARY_DIRS` `FILEPATH:C:/libssh2/lib`
  * `LIBSSH2_LIBRARIES` `STRING:libssh2`
  * XXX: embedded libssh2 is supported, but mandates CNG (Vista)
* You may need to copy `thread.c.obj` in both `src` and `src/win32` as
  `thread.obj`, unknown why.
* Investigate disabling WinHTTP for compatibility.
