<!-- REUSE-IgnoreStart -->
# Contributing to mx-guest-core

Read [README.md](README.md). Core holds OS-neutral protocol; independent correct implementations must agree byte for byte. Protocol changes follow the host implementation.

- Use freestanding C11 and only `stdint.h`, `stddef.h` and `string.h` from the standard library.
- Do not allocate, start threads, access files, call OS interfaces or perform floating-point arithmetic. Pass floating-point wire fields as integer bit patterns.
- Encode little-endian fields explicitly without packed wire structs.
- Validate inputs and return named refusals; production functions must not abort.
- List every C source and header once in `sources.list`.

The repository licence is MIT.

## Contributions

Read the [Developer Certificate of Origin 1.1](https://developercertificate.org) before signing off. Every commit requires a `Signed-off-by` trailer matching its author's name and email. Use `git commit -s`; the pull-request DCO workflow checks this requirement.

Write original implementation code. Do not paste or adapt code from other projects; use their supported interfaces. Record any introduced third-party material in `THIRD-PARTY-NOTICES`, retaining its original notices, licence identifier, copyright holders and source location. Discuss material under another licence before adding it.

## File notices and checks

New source files carry this repository's SPDX licence identifier and copyright notice in the file's comment syntax. Preserve existing notices; add a contributor's copyright when appropriate. Files that cannot carry comments are annotated in `REUSE.toml`.

Run the component checks described in [README.md](README.md) and `reuse lint` before submitting. REUSE runs on pushes and pull requests.

<!-- REUSE-IgnoreEnd -->
