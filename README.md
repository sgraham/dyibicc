# dyibicc: A Small DynASM JIT C Compiler

This is a fork of [chibicc](https://github.com/rui314/chibicc) which directly
generates machine code via [DynASM](https://luajit.org/dynasm.html).

I renamed it from chibicc to dyibicc to avoid confusion between the two. But the
code still overwhelmingly follows Rui's model and style.

Build and test with:

```
$ make
$ make test
```

Currently only supports Linux and x64. (It will build on Windows where it can
generate .dyos, but the calling convention is always SysV so they can't be run.)
