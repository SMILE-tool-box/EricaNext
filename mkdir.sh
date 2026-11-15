#!/usr/bin/env bash
# Compatibility entry point; use the maintained repository-local build script.
set -euo pipefail
echo "mkdir.sh is deprecated; use bash scripts/env/build_ericanext.sh" >&2
exec bash "$(dirname "$0")/scripts/env/build_ericanext.sh" "$@"
