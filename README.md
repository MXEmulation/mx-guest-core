<!-- REUSE-IgnoreStart -->
# mx-guest-core

Freestanding C11 encoders, decoders and validators for the MXGPU command protocol, MXSB shader bytecode and MXGA guest-agent frames.

## Protocol surface

- MXGPU negotiation, transport constants, command and completion records, resources, transfers, rendering, presentation and cursor updates.
- MXSB module record writers and validation for the supported vertex and fragment instruction subset.
- MXGA frames, system statistics, integration inventories and window actions.

Command and shader instruction coverage is incomplete. Public declarations are in [`include/`](include/), and protocol versions and PCI identities are in [`include/mx_versions.h`](include/mx_versions.h). Wire records are little-endian and validated before encoding.

Core contains OS-neutral protocol only: independent correct implementations must agree byte for byte, and disagreement must be a protocol violation. Scheduling, allocation, shader translation and OS integration belong to consumers. Core performs no allocation, OS calls or floating-point arithmetic; floating-point wire values use integer bit patterns. Protocol additions follow the host implementation.

## Build and tests

A C11 compiler and Make are required.

```sh
make test
```

The tests cover protocol records, MXSB verification, agent frames, integration records and cursor image, move and hide forms.

## Consumption

Consumers compile the `src/*.c` entries in [`sources.list`](sources.list) directly and include the public headers. The manifest lists every C source and header once, including tests; consumers do not compile the test entries. `CC`, `CFLAGS`, `CPPFLAGS` and `LDFLAGS` configure local builds.

[mx-guest-linux-kernel](https://github.com/MXEmulation/mx-guest-linux-kernel) and [mx-guest-linux-agent](https://github.com/MXEmulation/mx-guest-linux-agent) pin Core at `deps/core`. [mesa-mxgpu](https://github.com/MXEmulation/mesa-mxgpu) pins it through a Meson wrap. Consumers use Core's version constants and update their pins when the protocol changes.

## Licence

MIT. See [LICENCE](LICENCE) and [THIRD-PARTY-NOTICES](THIRD-PARTY-NOTICES). Contribution requirements are in [CONTRIBUTING.md](CONTRIBUTING.md).

<!-- REUSE-IgnoreEnd -->
