A GCC plugin to test for the existence of a name in *compile time*.

This plugin injects a function `uintptr_t __builtin_lookup_name(const char *name, uintptr_t default_)`.
Calls to this function are processed during compilation. The name `name` is looked up (using GCC's
`lookup_name` function). If found, the call is replaced with the found value. If not found, the call
is replaced with `default_`.

```
#include <stdio.h>

enum x {
    v1 = 1,
    // v2 = 2
};

void main(void) {
    enum x v1 = (enum x)__builtin_lookup_name("v1", (enum x)-1);
    enum x v2 = (enum x)__builtin_lookup_name("v2", (enum x)-1);

    printf("%d %d\n", v1, v2); // prints "1 -1"
}
```

Build with `make`, then run `gcc -fplugin=./builtin_lookup_name.so` to build the file.
See `tester.c` for a longer example.

This is called `__builtin_`, but it doesn't really behave like other GCC builtins. Real builtins are
processed during C parsing (e.g in `c-parser.c`), this way they can break C rules (for example,
`__builtin_choose_expr` which is a "function" that accepts arguments of arbitrary types).
I coudln't find a way to inject builtins as a plugin, so I resorted to injecting this as a real function.
and replace the call to it during compilation. This requires adding casts here and there.
It can be rewritten as a real builtin so no casts would be needed & it will work for absolutely all types.

I got the idea after needing something similar, and asking about it [in SO](https://stackoverflow.com/questions/63972541/compile-time-check-for-existence-of-an-enum-member).
