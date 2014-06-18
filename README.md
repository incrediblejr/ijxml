# ijxml #

ijxml is a minimalistic XML parser in C (inspired by [jsmn](http://zserge.com/jsmn.html))

Features
---

* compatible with C89
* no dependencies
* API contains only 2 functions
* no dynamic memory allocation

Design
---

ijxml operates on tokens which do not contain any data but points to boundaries (offsets) in the XML string.

Aux library
---

ijxml_aux is an optional lib with helper functions to query the parsed tokens from ijxml.

Usage
---

ijxml and ijxml_aux is implemented in single header files by a ["stb" design] (https://github.com/nothings/stb).

Add the files to your project and in _one_ source file use the define

__\#define IJXML\_IMPLEMENTATION__ (or IJXML\_AUX\_IMPLEMENTATION if using the aux library)

BEFORE the include, like this:

__\#define IJXML\_IMPLEMENTATION__

__\#include "ijxml.h"__

All other files should just #include "ijxml.h" (or ijxml\_aux.h) without the \#define.

Example
---

A [Premake](http://industriousone.com/premake) file is provided and some a basic tests/showcases is implemented in the __main.c__ file.