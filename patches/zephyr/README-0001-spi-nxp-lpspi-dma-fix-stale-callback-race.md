# README-0001-spi-nxp-lpspi-dma-fix-stale-callback-race

## What this patch is

`0001-spi-nxp-lpspi-dma-fix-stale-callback-race.patch` is a local patch against the Zephyr checkout under `nxp/zephyr`.

It updates Zephyr's NXP LPSPI SPI+DMA driver:

- Target file (in Zephyr): `drivers/spi/spi_nxp_lpspi/spi_nxp_lpspi_dma.c`

## Why it exists

On MCX (and generally with DMA-heavy systems), the DMA completion callback can be serviced:

- before the driver marks the transfer as "ongoing" (race), or
- after the driver already reset its internal state (late/stale interrupt)

This can produce noisy `spi_lpspi` errors/timeouts even when the SPI transfer itself succeeds.

## What it changes (high level)

- Arms the internal transfer state *before* starting RX/TX DMA so the callback never sees a NULL/idle state for an active transfer.
- Treats NULL and "done" states in the DMA callback as stale/late callbacks and ignores them.
- On timeout, aborts/cleans up DMA (disable SPI DMA requests + stop DMA channels) to avoid late callbacks desynchronizing subsequent transfers.

## How to apply

Run from the repo root:

```sh
bash ./apply_zephyr_patches.sh
```

Run it after `west update` (or any operation that refreshes `nxp/zephyr`).
