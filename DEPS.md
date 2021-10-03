* Try to build with Platform SDK's Windows 2000 build environment for manual
  NMake invokation.

* I try to avoid asm for the sake of CPU compatibility.

* Static seems to be fine, but requires additional steps.

* What I also do now is build the deps in their own prefixes then merge for
  making the build of Visual Git easier.

* I also link MSVCRT statically. `/MT` over `/MD`.

## zlib

If building a static zlib, you should change `/MD` to `/MT` in `Makefile.msc`,
so that it will link against the static CRT.

## OpenSSL

See also: http://wp.xin.at/archives/4479

* Use ActiveState Perl, Strawberry includes a GCC that will foul MSVC/CMake.

* An EP curve file needs patching, just init a value (because -Werror). Builds
  fine otherwise. (1.0)

* For 1.1.1, correct long long refs/constants, and make sure for NT 4,
  `CryptAcquireContext` is missing the silent option, or it will fail.

* I actually bounced up to 1.1.1, here are the options you need:

```shell
perl Configure vc-win32 --with-zlib-include=C:\zlib\include --with-zlib-lib=C:\zlib\lib\zlib.lib --prefix=C:\openssl -D_WIN32_WINNT=0x0406 no-dynamic-engine no-engine no-shared no-dso no-asm zlib no-async
```

* `zlib` instead of `zlib_dynamic`, even for dynamic builds. Point to the
  right `.lib` for dynamic or static.

* I disabled engines because it complicates both a relocatable dynamic and
  static setups. Means we're missing stuff like SChannel integration, but it
  matters less. Also because a func is missing on 2000/NT4.

* `no-async` is required for 2000, missing a fiber func.

* OpenSSL prefix should be installed to `c:/openssl` since CMake looks there.
  Probably could be changed.

* Not sure if I need to explicitly set the target SDK.

## libssh2

* NMake or VC++6 generator is fine. VC++6 makes it easier to fix any `/MD` to
  `/MT` flags though.

* For shared, `_WINDLL` defined or it'll foul exports, not generating a `.lib`.

* `vsnprintf` is weird, use same shim for `vscprintf` and prepare to fix refs,
  like in `misc.c`.

* If it doesn't find the OpenSSL library files, (IDK if this is due to 1.1.1
  or static), specify the `OPENSSL_ROOT` and maybe specify ssleay/libeay files
  for release mode manually from CMake GUI.

## libgit2

Patchset trying to be cleaned up and upstreamed, but notices for builders:

* VC++6 generator (NMake probably fine?)
* Use static CRT+no shared libs or not depending on sitch
* No Clar (involves Python)
* Use SSH, with additional variables because autodetect is dumb
  * `LIBSSH2_FOUND` `BOOL:YES`
  * `LIBSSH2_INCLUDE_DIRS` `FILEPATH:C:/libssh2/include`
  * `LIBSSH2_LIBRARY_DIRS` `FILEPATH:C:/libssh2/lib`
  * `LIBSSH2_LIBRARIES` `STRING:libssh2` (good for both static/shared)
  * XXX: embedded libssh2 is supported, but mandates CNG (Vista)
* You may need to copy `thread.c.obj` in both `src` and `src/win32` as
  `thread.obj`, unknown why.
* Disable WinHTTP for using OpenSSL. Using WinHTTP is probably preferable for
  Windows 7+. Available on 2000 or newer.

## Unicows

XXX: Investigate
