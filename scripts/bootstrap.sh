#!/usr/bin/env bash
set -euo pipefail
workspace_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$workspace_dir"
revision=2b0321db4a505a04c4e9aa47ed914758dd7aaf6c
checkout_dir="$workspace_dir/tools/decomp-wtf"
uv_command="$(command -v uv || true)"
if [[ -z "$uv_command" && -x "$HOME/.local/bin/uv" ]]; then
    uv_command="$HOME/.local/bin/uv"
fi
[[ -n "$uv_command" ]] || { echo 'Install uv in Ubuntu first: https://docs.astral.sh/uv/' >&2; exit 1; }
if [[ ! -d "$checkout_dir/.git" ]]; then
    git clone https://github.com/i12bp8/decomp.wtf.git "$checkout_dir"
    git -C "$checkout_dir" checkout --detach "$revision"
elif [[ "$(git -C "$checkout_dir" rev-parse HEAD)" != "$revision" ]]; then
    echo 'Existing decomp.wtf checkout has a different revision; leaving it untouched.' >&2
    exit 1
fi
# A checkout made by Windows Git may use CRLF; normalize this patched file.
python3 - "$checkout_dir/src/decompnet/rom/export_gba_build.py" <<'PY'
from pathlib import Path
import sys
path = Path(sys.argv[1])
path.write_bytes(path.read_bytes().replace(b'\r\n', b'\n'))
PY
if git -C "$checkout_dir" apply --reverse --check "$workspace_dir/patches/decomp-wtf-c-externals.patch" 2>/dev/null; then
    echo 'Both exporter patches already applied.'
else
    for patch_name in decomp-wtf-thumb-symbols.patch decomp-wtf-c-externals.patch; do
        patch_file="$workspace_dir/patches/$patch_name"
        if git -C "$checkout_dir" apply --reverse --check "$patch_file" 2>/dev/null; then
            echo "Exporter patch $patch_name already applied."
        elif git -C "$checkout_dir" apply --check "$patch_file"; then
            git -C "$checkout_dir" apply "$patch_file"
        else
            echo "Exporter patch $patch_name cannot be applied cleanly; stopping before applying it." >&2
            exit 1
        fi
    done
fi
export UV_CACHE_DIR="$workspace_dir/.cache/uv"
export UV_PROJECT_ENVIRONMENT="$workspace_dir/.venv-linux"
export DECOMPNET_TOOLCHAINS="$workspace_dir/.toolchains"
"$uv_command" sync --frozen --project "$checkout_dir"
"$workspace_dir/.venv-linux/bin/python" "$workspace_dir/scripts/decomp.py" setup
"$uv_command" pip install --python "$workspace_dir/.venv-linux/bin/python" -r "$workspace_dir/tests/requirements.txt"
"$workspace_dir/.toolchains/luvdis/venv/bin/python" "$workspace_dir/scripts/test_luvdis_stack.py"
echo 'Ready. From Windows run: .\decomp.ps1 import -OwnCopy'
