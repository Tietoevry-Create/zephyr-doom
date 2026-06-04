#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$script_dir"

zephyr_dir="$repo_root/nxp/zephyr"
patches_dir="$repo_root/patches/zephyr"

if [[ ! -d "$zephyr_dir/.git" ]]; then
  echo "error: expected Zephyr git checkout at $zephyr_dir" >&2
  exit 1
fi

if [[ ! -d "$patches_dir" ]]; then
  echo "error: expected patches directory at $patches_dir" >&2
  exit 1
fi

mapfile -t patch_files < <(find "$patches_dir" -maxdepth 1 -type f -name '*.patch' -print | sort)

if (( ${#patch_files[@]} == 0 )); then
  echo "No Zephyr patches found in $patches_dir"
  exit 0
fi

for p in "${patch_files[@]}"; do
  if git -C "$zephyr_dir" apply --reverse --check "$p" >/dev/null 2>&1; then
    echo "Already applied: $p"
    continue
  fi

  if ! git -C "$zephyr_dir" apply --check "$p"; then
    echo "error: patch does not apply: $p" >&2
    exit 1
  fi

  echo "Applying $p"
  git -C "$zephyr_dir" apply --whitespace=nowarn "$p"
done

echo "Zephyr patches applied."
