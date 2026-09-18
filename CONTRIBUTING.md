<!-- REUSE-IgnoreStart -->
# Contributing to mx-guest-core

Read [README.md](README.md) first. Most contributions that are turned away are turned away because they put implementation, or something specific to one operating system, into Core. The test in the README decides which side a change is on.

## Protocol changes follow the host

Core does not define the wire protocols; it carries the guest-side copy of protocols the MX host device implementation defines. A change that adds an opcode, a command, a feature bit, a record field or a version constant the host does not already implement will not be accepted. The order is always: the host gains the change, then Core gains the encoder and the constant.

## Engineering rules

Every change must keep to the rules in the README. In short:

- Freestanding C11. The only standard headers permitted are `stdint.h`, `stddef.h` and `string.h`.
- No allocation, threads, file access, OS calls or floating-point arithmetic. No `float` or `double` in any signature.
- No packed structs on the wire path. Encode and decode one field at a time with explicit shifts; the wire is little-endian.
- Every encoder validates its input against the host's acceptance rules and returns a named refusal rather than emitting bytes the host would reject.
- Every function is total. No aborting assertions.
- Every `.c` and `.h` file is listed in `sources.list` exactly once, in the same commit that adds it.

## Developer Certificate of Origin

Contributions are accepted under the Developer Certificate of Origin, version 1.1: https://developercertificate.org

Read the full text before signing off. Adding a sign-off to a commit is your certification of that text for that commit.

Sign off every commit with:

```
git commit -s
```

This appends a trailer of exactly this form to the commit message:

```
Signed-off-by: Name <email>
```

The name and email in the trailer must match the commit's author name and author email exactly, including case. The DCO check (`.github/workflows/dco.yml`) runs on every pull request, examines every non-merge commit in it, and fails the pull request if any commit lacks a `Signed-off-by` trailer equal to that commit's `Author Name <author email>`. The check runs only on pull requests. Maintainers who push directly to a branch must still sign off every commit; the requirement is the same whether or not the check runs.

`git commit -s` writes the trailer from your configured `user.name` and `user.email`. If the commit's author is someone else, for example when you commit a change on another person's behalf, the author must add their own sign-off. To add missing sign-offs to your own commits on a branch, use `git rebase --signoff <base>` or, for the last commit only, `git commit --amend -s --no-edit`, then force-push the branch.

## Licence headers

Every new source file carries a two-line SPDX header as its first lines. For C sources and headers:

```c
/* SPDX-License-Identifier: MIT */
/* SPDX-FileCopyrightText: 2026 Zak Noble-Clarke */
```

For files that use `#` comments, such as `Makefile` and shell scripts:

```
# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2026 Zak Noble-Clarke
```

If you hold copyright in your contribution to a file, add your own `SPDX-FileCopyrightText: <year> <name>` line below the existing ones. Never remove or alter an existing copyright or licence line.

Documentation and repository metadata that cannot carry a header are listed in `REUSE.toml`. Do not add a new header-less file without adding it there, and do not use `REUSE.toml` to avoid putting a header on a source file.

## REUSE compliance

`reuse lint` must pass. It runs in CI (`.github/workflows/reuse.yml`) on every push and pull request. Run it locally before pushing; the tool is described at https://reuse.software.

## Do not copy code from other projects

Write the code yourself. Do not paste or adapt code from Mesa, the Linux kernel, libdrm, or any other project, even where the licence would appear to permit it, and even for small helpers such as a ring walker or a little-endian field encoder.

The reason is specific to Core. Anything that lands here is offered to everyone under MIT, including code whose owner never agreed to that. Core is also compiled into the GPL-2.0-only kernel modules and agent programs and into the Mesa fork, so a single paste propagates into all of them and is hard to withdraw once released. Protocol-shaped code is exactly where an existing implementation is a tempting reference, because every ring walker and field encoder looks broadly alike.

A similarity gate that scans changes against a corpus of plausible third-party sources is planned for Core's CI. It is not implemented, and a clean run would not in any case prove provenance, since it can only find what is in its corpus. The rule stands on its own.

## Material under another licence

No material under a licence other than MIT may enter this repository without an entry in `THIRD-PARTY-NOTICES` in the same commit, naming the work, its SPDX identifier, its copyright holders and its source location, with the original notices kept intact. Given the rule above, raise any such case before opening a pull request; the expected answer is no.

<!-- REUSE-IgnoreEnd -->
