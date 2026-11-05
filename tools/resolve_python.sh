#!/usr/bin/env bash
# Resolve a Python 3 interpreter that can import Pillow from an MSYS2 shell.
# Supports MSYS2 Python, shell-visible Python, Windows PATH, and every
# interpreter registered with the Windows Python Launcher.
set -euo pipefail

requested="${1:-auto}"

is_usable() {
    local executable="$1"
    [[ -x "$executable" ]] && "$executable" -c 'import sys; from PIL import Image; assert sys.version_info >= (3, 10)' >/dev/null 2>&1
}

if [[ "$requested" != "auto" ]]; then
    explicit="$(command -v "$requested" 2>/dev/null || printf '%s' "$requested")"
    if is_usable "$explicit"; then
        printf '%s\n' "$explicit"
        exit 0
    fi
    printf 'ERROR: ASSET_PYTHON=%s cannot run Python 3.10+ with Pillow.\n' "$requested" >&2
    exit 1
fi

# Prefer a Python installation native to MSYS2 UCRT64, then other shell-visible
# Python installations.
for candidate in \
    /ucrt64/bin/python.exe \
    /mingw64/bin/python.exe; do
    if is_usable "$candidate"; then
        printf '%s\n' "$candidate"
        exit 0
    fi
done

for command_name in python3 python; do
    candidate="$(command -v "$command_name" 2>/dev/null || true)"
    if [[ -n "$candidate" ]] && is_usable "$candidate"; then
        printf '%s\n' "$candidate"
        exit 0
    fi
done

# Search Windows PATH. MSYS2 may not see a Python installation added to PATH
# after the terminal was started, but where.exe can still find inherited entries.
if command -v where.exe >/dev/null 2>&1 && command -v cygpath >/dev/null 2>&1; then
    while IFS= read -r windows_candidate; do
        [[ -z "$windows_candidate" ]] && continue
        [[ "$windows_candidate" == *"/WindowsApps/"* || "$windows_candidate" == *"\\WindowsApps\\"* ]] && continue
        candidate="$(cygpath -u "$windows_candidate")"
        if is_usable "$candidate"; then
            printf '%s\n' "$candidate"
            exit 0
        fi
    done < <(where.exe python.exe 2>/dev/null | tr -d '\r')
fi

# The Python Launcher knows about registered installations even when their
# directories are not on PATH. Enumerate all of them instead of testing only
# `py -3`, which selects the default version and can miss Pillow in another one.
launcher="$(command -v py.exe 2>/dev/null || command -v py 2>/dev/null || true)"
found_launcher_python=0
if [[ -n "$launcher" ]] && command -v cygpath >/dev/null 2>&1; then
    while IFS= read -r windows_candidate; do
        [[ -z "$windows_candidate" ]] && continue
        [[ "$windows_candidate" != *"python.exe" && "$windows_candidate" != *"pythonw.exe" ]] && continue
        found_launcher_python=1
        candidate="$(cygpath -u "$windows_candidate" 2>/dev/null || true)"
        if [[ -n "$candidate" ]] && is_usable "$candidate"; then
            printf '%s\n' "$candidate"
            exit 0
        fi
    done < <("$launcher" -0p 2>/dev/null | tr -d '\r' | sed -n 's/.*\([A-Za-z]:\\.*pythonw\?\.exe\)$/\1/p')
fi

printf '%s\n' 'ERROR: no usable Python 3.10+ interpreter with Pillow was found.' >&2
if (( found_launcher_python )); then
    printf '%s\n' 'Python installations were found through the Windows Python Launcher, but none could import Pillow.' >&2
else
    printf '%s\n' 'No registered Python installation was found through the Windows Python Launcher.' >&2
fi
printf '%s\n' 'Install Pillow for one of the registered interpreters, for example:' >&2
printf '%s\n' '  py -3.13 -m pip install Pillow' >&2
printf '%s\n' 'Then verify it with:' >&2
printf '%s\n' '  py -3.13 -c "from PIL import Image; print(Image.__version__)"' >&2
printf '%s\n' 'Alternatively pass the full interpreter path with ASSET_PYTHON.' >&2
exit 1
