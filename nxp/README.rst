NXP FRDM-MCXN947 port (vs. NRF5340 reference)
============================================

This folder contains the Zephyr application used to run **Doom** on the
**NXP FRDM-MCXN947** (MCXN947, Cortex-M33).

It is a port of the NRF5340 reference application found under ``zephyrdoom/``.
The goal of this port is to keep the Doom core as close as possible to the
reference, while swapping out the Nordic-specific hardware pieces.

Where to look
-------------

- Reference (NRF5340): ``zephyrdoom/``
- This port (NXP): ``nxp/``

Key differences from the NRF5340 build
-------------------------------------

**Hardware/driver layer**

- Nordic-specific modules (QSPI/XIP, buttons, joystick, I2S, etc.) are not used
   directly on NXP.
- The NXP build currently links a **minimal portable hardware layer** in
   ``nxp/src/portable_stubs.c`` and a Zephyr-based FT810 display driver in
   ``nxp/src/n_display.c``.
- ``nxp/CMakeLists.txt`` intentionally does **not** compile several Nordic
   modules (for example ``n_qspi.c``, ``n_buttons.c``, ``n_mem.c``), and provides
   replacements via ``portable_stubs.c``.

**Memory model**

- The NXP board has multiple SRAM banks. The port adjusts linker-visible SRAM
   to use more RAM than the default single-bank configuration.
- ``nxp/prj.conf`` sets ``CONFIG_SRAM_SIZE=416`` (KiB) for the application.
- ``nxp/boards/frdm_mcxn947_mcxn947_cpu0.overlay`` disables ``&sramg`` and
   ``&sramh`` so they don’t conflict with treating SRAM as one contiguous region.

**Display path (FT810 over SPI)**

- The display path is Zephyr SPI-based, targeting **32 MHz** and using **DMA**.
- DMA requires cache-coherency handling on MCXN947, so the port flushes D-cache
   ranges before DMA reads and chunks large transfers to avoid controller/DMA
   transfer-length limits.

**Storage / WAD / texture composites**

- The codebase expects an external-flash ("QSPI") region for two things:

  1. The **IWAD** (game data file), loaded at flash offset 0.
  2. **Composite wall textures**, generated at startup and stored in flash
     immediately after the WAD.

- On the NRF5340 reference the Nordic ``nrfx_qspi`` driver provides
  erase/write/read plus an XIP mapping at ``0x12000000``.  The WAD is
  transferred from an SD card to flash on first boot, and composite textures
  are written once (triggered by holding a button).  Both persist across
  reboots because flash is non-volatile.

- On this NXP port:

  - The WAD is **pre-loaded to external flash from a host PC** (no SD card).
    The ``doom1.wad`` binary is converted to Intel HEX with
    ``--change-addresses 0x80000000`` and flashed with ``pyocd``::

        arm-zephyr-eabi-objcopy -I binary -O ihex \
            --change-addresses 0x80000000 \
            gamedata/doom1.wad gamedata/qspi_nxp_80.hex

        pyocd flash -t MCXN947VDF gamedata/qspi_nxp_80.hex

  - Flash erase/write/read are implemented in ``nxp/src/portable_stubs.c``
    using Zephyr's standard ``<zephyr/drivers/flash.h>`` API against the
    board's W25Q64 FlexSPI NOR device (DT node ``w25q64jvssiq``).

  - ``N_qspi_data_pointer()`` returns ``EXT_FLASH_BASE + offset``
    (``0x80000000``), giving direct XIP read access to flash contents.

**Wall textures — composite generation**

Wall rendering uses *composite textures*: at init time, each wall texture is
assembled from its constituent patches and stored contiguously so the renderer
can read columns directly.  This is a separate step from loading the WAD.

The flow is controlled by the ``generate_to_flash`` flag in
``nxp/src/doom/r_data.c`` (function ``R_GenerateInit``):

- When **``true``**: ``R_GenerateComposite_N()`` composites each wall texture
  into a temporary RAM buffer and then writes it to external flash via
  ``N_qspi_write()``.  This is slow (many flash erase + write cycles) but
  only needs to happen **once** — the data persists in non-volatile flash.

- When **``false``** (default): the flash write step is skipped.  The code
  assumes composites are already present in flash from a previous
  ``generate_to_flash = true`` boot.

**If wall textures look wrong** (solid brown / pink artefacts), set
``generate_to_flash = true``, rebuild, flash, and boot once.  After the
composites are generated, set it back to ``false`` to restore normal boot
speed.

On the NRF5340 reference this toggle was originally wired to a button press;
on this NXP port it is a compile-time flag.

Controls (minimal)
------------------

This port supports **2 on-board buttons** as a minimal control scheme:

- **SW3** (DT alias ``sw1``):
   - hold: ``KEY_UPARROW`` (move forward / menu up)
   - long-press (~600 ms): ``KEY_ESCAPE`` (open menu / back)

- **SW2** (DT alias ``sw0``):
   - short press: ``KEY_ENTER`` (menu select) + ``KEY_RCTRL`` (fire)
   - long-press (~600 ms): ``' '`` (use/open doors)

Implementation details:

- GPIO polling + debounce + long-press logic lives in ``nxp/src/portable_stubs.c``.
- The on-board button nodes are enabled in
   ``nxp/boards/frdm_mcxn947_mcxn947_cpu0.overlay``.

Build and flash
---------------

Build from the repository root:

.. code-block:: sh

    west build -b frdm_mcxn947_mcxn947_cpu0 -p auto nxp

Or from inside the ``nxp`` folder:

.. code-block:: sh

    west build -b frdm_mcxn947_mcxn947_cpu0 -p auto .

Flashing depends on your environment (MCU-Link, LinkServer, etc.). Use your
existing workflow for the board; this README focuses on what changed in the
application.

Summary of changes on this branch
---------------------------------

The commit history on ``feature/Markek1/nxp_port`` shows the rough progression:

- **Initial NXP application import** ("Basic core Doom")
   - Created the ``nxp/`` Zephyr app, largely mirroring the reference Doom core.
- **Display bring-up** ("Display working")
   - Added/modified ``nxp/src/n_display.c`` and board overlay to get FT810 output.
- **Crash fixes (memory/alloc)** ("Fix crash on allocation", "Fix crash")
   - Adjusted ``nxp/prj.conf`` and stubs to stabilize startup and runtime.
- **Performance improvements** ("30FPS now", CPU usage investigation)
   - Switched toward DMA-backed SPI transfers; reduced overhead; reached
      ~34 FPS (near the 35 Hz Doom tic ceiling).

What we changed in this chat session (high level)
-------------------------------------------------

- **Allocator correctness**: fixed heap corruption caused by mixing different
   allocators by making ``N_malloc`` use libc ``malloc()`` to match ``free()``.
- **SRAM usage**: ensured the NXP build can use more SRAM (``CONFIG_SRAM_SIZE``
   and SRAM bank handling in the overlay).
- **SPI DMA correctness**: fixed partial-frame issues by:
   - flushing D-cache for DMA reads
   - chunking large transfers
   - ensuring async transfer completion/in-progress handling is correct
- **QSPI stub behavior**: implemented correct 64 KiB block reservation/allocation
   bookkeeping so “allocated” regions don’t all overlap at offset 0.
- **Basic buttons**: implemented SW2/SW3 polling + mapping (see "Controls").
- **Wall textures (composite generation)**: replaced no-op QSPI stubs with real
   Zephyr flash API calls (``flash_erase``, ``flash_write``, ``flash_read``)
   against the on-board W25Q64 FlexSPI NOR flash.  Set ``generate_to_flash =
   true`` for one boot to populate the composite texture region in flash.
   Wall textures now render correctly.

Current limitations / known issues
----------------------------------

- ``generate_to_flash`` is a compile-time flag.  After re-flashing the WAD
   or erasing external flash you must do one build+boot with
   ``generate_to_flash = true`` to regenerate composites, then set it back to
   ``false``.
- Audio is not implemented (I2S stubs are no-ops).
- Bluetooth / gamepad input is not implemented (stub only).
- Only two on-board buttons are mapped; no joystick / analog input.

