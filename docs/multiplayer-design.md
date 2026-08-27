# zephyr-doom multiplayer: design notes

Chocolate Doom 3.0.0-compatible UDP multiplayer, on three targets:

- **native_sim**: the Linux build joins a stock `chocolate-server` and renders
  in its SDL window. Works with 3-4 players in any mix of native_sim and desktop
  `chocolate-doom`, connect order independent.
- **FRDM-MCXN947**: the board joins over real Ethernet (ENET-QOS). Confirmed in
  a 2-player game against native_sim.
- **nRF5340 DK**: no Ethernet port, so the board joins over **USB CDC-ECM**
  (it enumerates as a USB network adapter on the host). Same static-IP scheme
  and boot-time SP/MP decision as the NXP board.

Join only; native/board can't host a game (`net_server.c` is stubbed).

> **Setup and run instructions** (installing chocolate-doom, running the relay
> server, and the per-target network setup) live in `docs/multiplayer.adoc`.
> This file is the design rationale and the debugging history behind the code.

## How it works

`net_zephyr.c` is the UDP transport over Zephyr `zsock_*`, identical on both
targets. The difference is underneath: native_sim uses host-offloaded sockets
(`NET_NATIVE_OFFLOADED_SOCKETS`, borrows the Linux stack, no TAP/root); the board
runs the real Zephyr IP stack on the on-chip MAC/PHY.

`CONFIG_FEATURE_DOOM_NET` gates the netcode and compiles the real `net_client.c`.
Handshake (`net_client.c`): connect -> LAUNCH -> client sends settings
(`GAMESTART`) -> server broadcasts consolidated `GAMESTART` -> in game. The first
peer to connect is the controller (`consoleplayer 0`) and its settings win.

Sync (`d_loop.c`): strict 35 Hz lockstep, `gametic` advances only up to
`lowtic = min(maketic, recvtic)`. Default old sync; `-newsync` flips it.

## native_sim: notable fixes

**Lag was the desktop, not the netcode.** A stock desktop peer on GNOME/Wayland
felt ~1s behind; instrumentation showed the tics healthy (35/s, ~6ms latency).
SDL2's Wayland backend starts in a bad vsync-pacing state. Fix: force X11
(`SDL_VIDEODRIVER=x11`, or `video_driver "x11"` in the config). Not a zephyr-doom
issue.

**`players[N].mo` garbage for non-zero players (`boolean` ODR bug).** `boolean`
was `uint8_t` (1 byte) in TUs that had already pulled in `<stdbool.h>` (e.g. via
`<zephyr/kernel.h>`) and a 4-byte `enum` elsewhere, so `sizeof(player_t)`
differed per file (220 vs 250). `players[]` stride disagreed between writer and
reader; index 0 was immune, index >=1 landed at the wrong address. Fix: include
`<stdbool.h>` unconditionally in `doomtype.h` so `boolean` is always 1 byte.

## NXP: bringing up hardware multiplayer

Enabled in `boards/frdm_mcxn947_mcxn947_cpu0.conf`: `FEATURE_DOOM_NET`,
`NET_L2_ETHERNET`, IPv4/UDP, static IP `192.168.10.2`. Do not enable
`CONFIG_POSIX_API`, because it force-selects `POSIX_TIMERS`, whose `timer.c` doesn't
build against this tree's picolibc. The netcode is already POSIX-free.

**RAM.** ~416 KB total, and Doom's two framebuffers take 128 KB. Doom's zone
allocator draws from the leftover-RAM malloc arena, so networking's static data
starved it and it OOM'd at `ST_Init`. Reclaimed by dropping `NET_MAXPLAYERS` to 4
on hardware (Doom caps at 4 anyway; native_sim keeps 8) and shrinking the k_heap.
Sits at ~84%.

**Two bugs blocked the netgame, both fixed:**

- *Fatal socket errors.* `NET_Zephyr_SendPacket`/`RecvPacket` called `I_Error`
  (which exits) on any socket error. A brief link blip -> `sendto` fails
  `errno 115` -> the game crashed. UDP is lossy and Doom retransmits, so these
  are now non-fatal (drop the packet), matching upstream `net_sdl.c`.
- *Spurious PHY link-down.* The `phy_mii` monitor polls the PHY over MDIO every
  500 ms. Under Doom's steady netgame traffic the poll races the MAC and
  mis-reads "link down", which tears down TX mid-game though the link is
  physically fine (frames keep arriving). Worked around with
  `CONFIG_PHY_MONITOR_PERIOD=3600000` so it doesn't re-poll during a session.
  Proper fix would be a devicetree `fixed-link`.

**Display vs network ordering.** The FT810 power-on briefly drops the PHY link.
`I_InitGraphics` is called early (before the handshake) and is idempotent, so the
glitch and recovery happen before the game is live; a short carrier wait follows.

## nRF5340: multiplayer over USB CDC-ECM

The nRF5340 DK has no Ethernet port, so it nets over **USB CDC-ECM** instead: the
board presents a USB network adapter to the host, and the same Zephyr IP stack +
`net_zephyr.c` transport run on top of it (the netcode is transport-agnostic).

- Enabled in `boards/nrf5340dk_nrf5340_cpuapp.conf`: `FEATURE_DOOM_NET`, the USB
  device stack (`USB_DEVICE_STACK_NEXT`, `USBD_CDC_ECM_CLASS`), `NET_L2_ETHERNET`,
  IPv4/UDP, static IP `192.168.10.2` / GW `192.168.10.1`.
- `src/n_usb.c` (`N_usb_net_init`, called from `main.c`) builds the USB device
  (VID:PID `2fe3:0001`, "zephyr-doom net") and registers the CDC-ECM class; the
  overlay declares the `zephyr,cdc-ecm-ethernet` node.
- `CONFIG_DOOM_NET_LINK_WAIT_MS=8000` (vs 3000 on NXP) to give USB enumeration
  time before the boot-time SP-vs-MP decision.
- Host side: the board enumerates as e.g. `enxXXXXXXXXXXXX`; put it on the cable
  subnet (`sudo ip addr add 192.168.10.1/24 dev <iface>`) and run the server
  there. Built with the nRF Connect SDK toolchain.

## Known-open

- Board display SPI is flaky (`spi_lpspi` DMA errors, `Display ID: FF` at init).
  Pre-existing, cosmetic, doesn't affect networking. Not yet chased down.
- Hosting via `-server`: dead until `net_server.c` (`NET_SV_*`) is implemented.
- `W_Checksum` stubbed -> harmless "WAD SHA1 does not match server" warning.
- Hardware games are fixed at 2 players: `M_ArgvInit` injects `-nodes 2` and
  there is no way to change it short of rebuilding.
- Proper fix for the PHY mis-read on FRDM would be a devicetree `fixed-link`
  (or MDIO/MAC locking in `eth_nxp_enet_qos`) instead of stretching
  `CONFIG_PHY_MONITOR_PERIOD`.

## Respawn

`G_DoReborn`'s netgame branch was commented out in the fork, so a dead player
never respawned while a stock peer did. With the consistency check live (see
above) the divergence was fatal: `consistency failure (155 should be 0)` and a
disconnect on the first respawn of a 2-player game. Restored from upstream.

Restoring it also made the corpse queue live for the first time, which exposed
two more divergences from a stock peer, both now fixed: `BODYQUESIZE` was cut
from 32 to 8 (corpses are collidable and `G_CheckSpot` tests against them when
picking a spawn point), and `bodyqueslot` was a `byte`, which wraps at 256
respawns and would then skip removals the peer still performs.

## Key files

- `src/net_zephyr.c`: UDP transport (offloaded on native_sim, real stack on HW).
- `src/net_client.c`: client handshake.
- `src/d_loop.c`: lockstep, `TryRunTics`; single-player fallback on connect fail.
- `src/m_argv.c`: hardware boot decides SP vs MP from the Ethernet carrier.
- `src/doom/d_main.c`: `-warp` fix, early display init + link wait.
- `src/doomtype.h`: `boolean` ODR fix.
- `src/n_usb.c`: nRF5340 USB CDC-ECM bring-up.
- `boards/frdm_mcxn947_mcxn947_cpu0.conf`: NXP networking + RAM tuning.
- `boards/nrf5340dk_nrf5340_cpuapp.{conf,overlay}`: USB CDC-ECM networking.
- `boards/native_sim_native_64.{conf,overlay}`: SDL display + offloaded sockets.
- `src/net_client_stub.c`: no-op stand-ins built when `FEATURE_DOOM_NET=n`.
- `src/net_gui_headless.c`: replaces upstream's libtextscreen lobby.
