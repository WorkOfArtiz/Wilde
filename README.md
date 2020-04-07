# Wilde

This is a secure allocator for the unikraft kernel still in active development
at the time of writing.

## Supported:

```
Unikraft: Iapetus 0.3.1 (custom patched)
Architecture: x86_64 (hardware registers)
Platform: KVM
```

## Features

- Invalid free detection
- Double free detection
- Use-after-free detection
- [Optional] Coarse grained buffer overflow detection
- [Optional] Fine grained buffer overflow detection
- [Optional] Locking
- [Optional] Arbitrary memory initialisation
- [Optional] Metadata protection
- [Optional] Dynamic allocation logging and resolving
- [Optional] ASLR, only for allocated objects like the stack, not code pages and double mapping still exists.

