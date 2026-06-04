#!/usr/bin/env bash

set -euo pipefail

# Prepare Python virtual environment if not present
if [ ! -d .venv ]; then
    python3 -m venv .venv
    # shellcheck disable=SC1091
    source .venv/bin/activate

    # Install required pip packages for project development
    pip install --use-pep517 -U pip
    pip install --use-pep517 -U -r requirements.txt
fi

# shellcheck disable=SC1091
source .venv/bin/activate
