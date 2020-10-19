# Hex-Rays OLLVM Deobfuscator and MicroCode Explorer

Original by Rolf Folles
Ported from [https://github.com/RolfRolles/HexRaysDeob](https://github.com/RolfRolles/HexRaysDeob)

Reference: [https://www.hex-rays.com/blog/hex-rays-microcode-api-vs-obfuscating-compiler/](https://www.hex-rays.com/blog/hex-rays-microcode-api-vs-obfuscating-compiler/)

* Implements all options in a menu item on pseudo code view

* Allows to runtime enable/disable the deobfuscator

* After enable/disable press F5 again in the pseuco code view to refresh

This uses the new C++ plugin API so it's only >= IDA 7.5 compatible

Based on IDA SDK ht_view sample plugin

Alternative microcode explorer with different features is Lucid

* [https://github.com/gaasedelen/lucid](https://github.com/gaasedelen/lucid)

* [https://blog.ret2.io/2020/09/11/lucid-hexrays-microcode-explorer/](https://blog.ret2.io/2020/09/11/lucid-hexrays-microcode-explorer/)

# Build

The default `Makefile` is for macOS version. Windows and Linux versions available per original project.

# Mac build

Edit the `Makefile` and fix the IDA paths if necessary.

To compile and install 64 bit version:
```
EA=1 make
EA=1 make install
```

To compile and install 32 bit version:
```
EA=0 make
EA=0 make install
```

# Linux build

To compile and install 64 bit version:
```
EA=1 IDA_DIR=<PATH_TO_IDA> IDA_SDK=<PATH_TO_IDA_SDK> make -f makefile.lnx
EA=1 IDA_DIR=<PATH_TO_IDA> IDA_SDK=<PATH_TO_IDA_SDK> make install -f makefile.lnx
```

To compile and install 32 bit version:
```
EA=0 IDA_DIR=<PATH_TO_IDA> IDA_SDK=<PATH_TO_IDA_SDK> make -f makefile.lnx
EA=0 IDA_DIR=<PATH_TO_IDA> IDA_SDK=<PATH_TO_IDA_SDK> make install -f makefile.lnx
```

# Windows build

Open the Visual Studio and hope for the best. Didn't test :-).
