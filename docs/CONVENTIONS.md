# Conventions

## Commits

- English, Conventional Commits style: `type(scope): summary`, imperative and
  lowercase, no trailing period. Types in use: `feat`, `fix`, `docs`, `chore`.
- Body as a bullet list of what changed and why, with the root cause when a bug
  is fixed. End with factual lines such as the firmware size, memory usage and
  the command that verified the change.
- One commit per verified milestone.

## Layout

| Path | Contents |
|------|----------|
| `port/` | port layer the emulator talks to: console backends, RAM disk, shims |
| `sdk_probe/` | firmware project for the SF32LB525 board (scons) |
| `third_party/8086tiny/` | vendored emulator, its BIOS and the disk images |
| `tools/` | host utilities, the floppy image builder and the regressions |
| `docs/` | project conventions |

## Generated files

Build outputs are ignored and never committed: `sdk_probe/project/build_*/`,
`.sconsign.dblite`, the `*.o` files SCons leaves next to sources outside the
build directory, and `__pycache__/`. Rebuild the firmware with:

```sh
cd sdk_probe/project && scons --board=sf32lb52-lchspi-ulp -j8
```
