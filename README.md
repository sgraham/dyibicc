# dyibicc: A Small DynASM JIT C Compiler

This is a fork of [chibicc](https://github.com/rui314/chibicc) which directly
generates machine code via [DynASM](https://luajit.org/dynasm.html).

I renamed it from chibicc to dyibicc to avoid confusion between the two. But the
code still overwhelmingly follows Rui's model and style.

Currently only supports Linux and Windows, and x64 only.

Build and test on Linux with:

```
$ ./m r
$ ./m r test
```

or on Windows (from a VS x64 cmd):

```
> m r
> m r test
```

`r` means Release, and can also be `d` for Debug or `a` for ASAN.

For example, using `m a test -j1` would run the tests under an ASAN build, one
at a time (the extra args `-j1` are passed to the sub-ninja invocation).
