# Zephyr patches

This project keeps a small number of local fixes as patch files (instead of committing changes directly into the Zephyr checkout).

## When to apply

Run after a fresh clone / `west update`, or any time `nxp/zephyr` is re-synced.

## Apply

```sh
bash ./apply_zephyr_patches.sh
```

## Notes

- Patches are applied to the Zephyr git checkout located at `nxp/zephyr`.
- If a patch no longer applies (because upstream Zephyr changed), you’ll need to refresh the patch (or switch to a pinned fork in `west.yml`).
