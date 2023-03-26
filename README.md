# dyibicc: A Small DynASM JIT C Compiler

This is a fork of [chibicc](https://github.com/rui314/chibicc) which directly
generates machine code via [DynASM](https://luajit.org/dynasm.html).

I renamed it from chibicc to dyibicc to avoid confusion between the two. But the
code still overwhelmingly follows Rui's model and style.

Build and test on Linux with:

```
$ make
$ make test
```

or on Windows (from a VS x64 cmd):

```
> make
> make test
```

Currently only supports Linux or Windows, and x64 only.
