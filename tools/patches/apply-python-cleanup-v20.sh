#!/usr/bin/env bash
set -euo pipefail

# VitaOS patch v20: keep Python out of runtime/build paths.
# Uses Bash/coreutils/perl only. No Python is required.

ROOT=$(pwd)
README="$ROOT/README.md"
PATCH_DIR="$ROOT/tools/patches"
DOC_DIR="$ROOT/docs/tools"
VERIFY="$PATCH_DIR/verify-no-python-runtime.sh"

if [[ ! -f "$ROOT/Makefile" || ! -d "$ROOT/kernel" || ! -d "$ROOT/arch" ]]; then
  echo "[v20] ERROR: run this from the VitaOS repository root" >&2
  exit 1
fi

mkdir -p "$PATCH_DIR" "$DOC_DIR"

if [[ -f "$README" ]]; then
  # Remove stale hosted requirement line. The current Makefile does not use Python for hosted.
  perl -0pi -e 's/\n- `python3`\n/\n/g; s/\n- python3\n/\n/g' "$README"

  if ! grep -q 'Python is not a VitaOS runtime or build dependency' "$README"; then
    perl -0pi -e 's/(### Para hosted\n(?:- .+\n)+)/$1\nNota: Python is not a VitaOS runtime or build dependency. Old Python patch helpers are legacy development artifacts and must not be required by `make`, `make hosted`, `make smoke-audit`, `make smoke` or `make iso`.\n/s' "$README"
  fi
fi

cat > "$DOC_DIR/no-python-runtime.md" <<'DOC'
# No Python runtime in VitaOS

VitaOS F1A/F1B is C/freestanding-friendly and must not depend on Python inside the OS image or `BOOTX64.EFI`.

## Classification

- `kernel/`, `include/`, `arch/`, `drivers/`, `proto/`: runtime or freestanding-facing code. Python is not allowed.
- `Makefile`, `tools/build/`, `tools/image/`, `tools/test/`: build and smoke paths. Python must not be required.
- `tools/patches/*.py`: legacy development patch helpers from earlier Linux VM patching sessions. They are not VitaOS runtime code and are removed by the v20 cleanup patch.
- Documentation may mention Python only to state that it is not part of VitaOS runtime/build scope.

## Rule

Do not add Python3 to VitaOS, the UEFI image, the kernel, or mandatory build/test paths for the current milestone.

Use Bash shell helpers for development patches when a helper script is unavoidable.
DOC

cat > "$VERIFY" <<'VERIFY'
#!/usr/bin/env bash
set -euo pipefail

ROOT=$(pwd)

if [[ ! -f "$ROOT/Makefile" || ! -d "$ROOT/kernel" || ! -d "$ROOT/arch" ]]; then
  echo "[verify-python] ERROR: run this from the VitaOS repository root" >&2
  exit 1
fi

status=0

echo "[verify-python] checking Python files outside ignored build/git areas"
mapfile -t py_files < <(find . \
  -path './.git' -prune -o \
  -path './build' -prune -o \
  -name '*.py' -print | sort)

if (( ${#py_files[@]} > 0 )); then
  printf '%s\n' "${py_files[@]}"
  echo "[verify-python] ERROR: .py files remain in the repository tree" >&2
  status=1
else
  echo "[verify-python] OK: no .py files found"
fi

echo "[verify-python] checking mandatory runtime/build paths for python references"
check_paths=(Makefile arch include kernel drivers proto tools/build tools/image tools/test tests schema)
existing=()
for p in "${check_paths[@]}"; do
  [[ -e "$p" ]] && existing+=("$p")
done

if (( ${#existing[@]} > 0 )); then
  if grep -RInE '(^#!.*python|python3|[^A-Za-z0-9_]python[^A-Za-z0-9_])' "${existing[@]}" 2>/dev/null; then
    echo "[verify-python] ERROR: Python reference found in runtime/build path" >&2
    status=1
  else
    echo "[verify-python] OK: no runtime/build Python references"
  fi
fi

exit "$status"
VERIFY
chmod +x "$VERIFY"

removed=0
if [[ -d "$PATCH_DIR" ]]; then
  while IFS= read -r -d '' py; do
    echo "[v20] removing legacy Python patch helper: ${py#"$ROOT/"}"
    if command -v git >/dev/null 2>&1 && git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
      git rm -f -- "$py" >/dev/null 2>&1 || rm -f -- "$py"
    else
      rm -f -- "$py"
    fi
    removed=$((removed + 1))
  done < <(find "$PATCH_DIR" -maxdepth 1 -type f -name '*.py' -print0)
fi

bash "$VERIFY"

echo "[v20] Python cleanup applied; removed legacy .py patch helpers: $removed"
