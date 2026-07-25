**Title:** App packager ignores `R_ARM_ABS32` relocations in `.rel.text`, causing newlib `sin()`/`cos()` to fault on hardware

## Summary

Calling `sin()` or `cos()` from a watchapp with a medium-sized argument that
reaches newlib's lookup-table reduction path (for example `10.0` radians)
reliably hard-faults on hardware. The fault is in
`__ieee754_rem_pio2`, which reads a static lookup table through an absolute
address stored in a `.text` literal pool. That address is never relocated at
load time, so the app dereferences a link-time address instead of one relative
to where it was actually loaded.

This is not specific to `sin()` — it can affect any linked object that leaves an
`R_ARM_ABS32` reference in `.text`, rather than addressing its data through the
GOT.

## Environment

- SDK 4.17, `pebble-tool` 5.0.39
- Platform `emery` (Pebble Time 2 hardware); also built for `gabbro`
- Toolchain: xPack GNU Arm Embedded GCC 14.2.1

## Reproduction

Minimal `src/c/main.c`:

```c
#include <pebble.h>
#include <math.h>

int main(void) {
  // Above 9*pi/4 and below the large-argument path: this reaches npio2_hw.
  volatile double angle = 10.0;
  volatile double result = sin(angle);
  APP_LOG(APP_LOG_LEVEL_INFO, "sin(10.0) = %d milli", (int)(result * 1000));
  app_event_loop();
}
```

`pebble build && pebble install` on hardware. Smaller arguments can take one of
newlib's specialized reduction paths and do not necessarily access
`npio2_hw`; `5.0`, for example, is not a reliable reproducer for this specific
load.

## Observed

```
[11:40:23] fault_handling.c:105> App fault! {…} PC: 0x59ca LR: 0x1
```

`addr2line` on the app ELF resolves `0x59ca` to
`__ieee754_rem_pio2`.

Disassembly at the fault site:

```
59c6:  ldr    r3, [pc, #136]            ; loads the address of npio2_hw
59c8:  subs   r2, r6, #1
59ca:  ldr.w  r3, [r3, r2, lsl #2]      ; <-- faults here
```

## Root cause

In the minimal app above:

| Symbol | Link-time address |
|---|---|
| `npio2_hw` | `0x1b48` |
| `two_over_pi` | `0x1bc8` |

Literal pool slots inside `__ieee754_rem_pio2`, read straight out of the built
`pebble-app.bin`:

| Slot | Word | Kind | In app reloc table |
|---|---|---|---|
| `0x0f18` | `0x00001bc8` | address of `two_over_pi` | **no** |
| `0x0f20` | `0x00001b48` | address of `npio2_hw` | **no** |
| `0x0f00`, `0x0f04`, `0x0f08`, `0x0f0c`, `0x0f10`, `0x0f14`, `0x0f1c`, `0x10a8` | `0x3fe921fb` etc. | double constants | n/a |

The ELF does carry `R_ARM_ABS32` relocations for these two slots, but the app's
own relocation list is empty (`num_reloc_entries == 0`), so neither is fixed
up.

The reason is in `sdk/tools/inject_metadata.py`. `get_relocate_entries()`
collects exactly two things:

- relocations from sections whose name starts with `.rel.data`
  ([line 190](https://github.com/coredevices/PebbleOS/blob/main/sdk/tools/inject_metadata.py#L190))
- every word of the `.got` section

`.rel.text` is never read. That holds for app code, which the SDK compiles
position-independent so its global references go through the GOT — but not for
the prebuilt newlib objects pulled in from the toolchain, which address their
own static tables with plain absolute literals.

Checked against a real watchface for comparison: its packaged relocation table
contained 807 entries collected from `.rel.data` and `.got`. Its ELF contained
5389 relocations in total across all sections and relocation types; that total
is not the number that would need to be added. The relevant observation is that
none of its `.rel.text` `R_ARM_ABS32` entries appeared in the packaged
relocation table.

## Note on the emulator

This does not reproduce under `pebble install --emulator`: the app renders
normally there and faults on hardware. I have not determined whether QEMU loads
the app at its linked address or merely leaves the unrelocated low address
readable, but either behaviour masks the missing relocation and makes the
failure easy to miss during development.

## Workaround

Audit the linked ELF and avoid routines that leave `R_ARM_ABS32` references in
`.rel.text`. In my case I replaced double-precision
`sin`/`cos`/`asin`/`atan2` with the SDK's own `sin_lookup` and `cos_lookup`,
restructuring the maths to work on direction cosines so the inverse functions
were not needed.

I also removed `sqrt` and `fmod`: although they do not use the same trigonometric
lookup tables, the toolchain's implementations contain their own absolute
references to static constants in `.text`, so they cannot be assumed safe under
the current packaging rules.

## Possible directions

I do not know which of these fits your constraints, so treating this as a report
rather than a proposal:

1. Emit `.rel.text` `R_ARM_ABS32` relocations into the app relocation table
   alongside the `.data` and `.got` ones. Only entries of the type that needs
   load-base adjustment should be emitted, not every relocation in
   `.rel.text`. The cost is four bytes per additional `R_ARM_ABS32` entry and
   depends on the library objects linked into the app.
2. Build the SDK-side libm with GOT-relative addressing so its internal tables
   go through the same mechanism app code already uses.
3. If neither is practical, rejecting or warning at link time when a
   non-relocatable absolute reference survives in `.text` would at least turn a
   silent hardware-only crash into a build error.
