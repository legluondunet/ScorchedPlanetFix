#!/usr/bin/env bash
# Read-only disassembly of the user's own Scorched Planet executable.
set -Eeuo pipefail
export LC_ALL=C

usage() {
  cat <<'EOF'
Usage: inspect-go-hotspots-linux.sh [SPVOODOO.EXE] [NEW_REPORT.txt]

Run from the game directory. Inspects the EXE but never executes/patches it.
Requires GNU binutils objdump and sha256sum.
The address ranges are based on live perf samples, and are not patches.
Extended analysis includes 0x4495C0, the function that decides when the
0x427D24..0x427D38 loop ends, and its direct callers.
EOF
}

if [[ "${1:-}" == "--help" || "${1:-}" == "-h" ]]; then usage; exit 0; fi
exe="${1:-./SPVOODOO.EXE}"
[[ -f "$exe" && -r "$exe" ]] || { echo "Fichier illisible/introuvable : $exe" >&2; exit 1; }
command -v objdump >/dev/null 2>&1 || { echo "objdump (binutils) requis" >&2; exit 1; }
command -v sha256sum >/dev/null 2>&1 || { echo "sha256sum requis" >&2; exit 1; }
expected='fc1374b82b423a9836a9dda496da9960967fd75c3d98c5a0ef39d9fa8be0f80a'
actual="$(sha256sum -- "$exe")"
actual="${actual%% *}"
report="${2:-./ScorchedPlanetFix-Go-code-$(date +%Y%m%d-%H%M%S)-$$.txt}"
[[ ! -e "$report" ]] || { echo "Rapport existe deja : $report" >&2; exit 1; }

if ! objdump -f -- "$exe" >/dev/null 2>&1; then
  echo "objdump ne reconnaît pas le binaire : un objdump avec support PE-i386 est nécessaire." >&2
  exit 1
fi
{
  printf 'Scorched Planet (1996) - READ-ONLY static code inspection\n'
  printf 'File: %s\nSHA256: %s\nExpected SHA256: %s\n' "$exe" "$actual" "$expected"
  if [[ "$actual" == "$expected" ]]; then
    echo 'File identity: matches user-provided test build.'
  else
    echo 'WARNING: different binary; sampled virtual addresses may not apply.'
  fi
  printf '\n=== Object format ===\n'
  objdump -f -- "$exe"
  printf '\n=== Sections and their virtual addresses ===\n'
  objdump -h -- "$exe"
  printf '\n=== Imported timing / waiting / sound API names ===\n'
  objdump -p -- "$exe" 2>/dev/null |
    grep -Ei 'QueryPerformanceCounter|QueryPerformanceFrequency|timeGetTime|GetTickCount|Sleep|WaitFor|DirectSound|waveOut|GetSystemTime|SetTimer|DLL Name' |
    head -n 100 || true
  printf '\n=== Hotspot 0x00432b62: repeated perf samples ===\n'
  objdump -d -M intel --start-address=0x00432a80 --stop-address=0x00432c40 -- "$exe"
  printf '\n=== Hotspot 0x0043610d: nearby sampled code ===\n'
  objdump -d -M intel --start-address=0x004360b0 --stop-address=0x00436190 -- "$exe"
  printf '\n=== Hotspot 0x00427d38: rare sample ===\n'
  objdump -d -M intel --start-address=0x00427cd0 --stop-address=0x00427dd0 -- "$exe"
  printf '\n=== Function 0x004495c0: return value controls a potential busy wait ===\n'
  objdump -d -M intel --start-address=0x00449560 --stop-address=0x004497e0 -- "$exe"
  printf '\n=== Adjacent helper 0x00449450 called during initialization ===\n'
  objdump -d -M intel --start-address=0x004493e0 --stop-address=0x00449560 -- "$exe"
  printf '\n=== Caller context 0x00427c00-0x00427e40 (includes loop 0x427d24-38) ===\n'
  objdump -d -M intel --start-address=0x00427c00 --stop-address=0x00427e40 -- "$exe"
  printf '\n=== Timer initialization context 0x00432800-0x00432a20 ===\n'
  objdump -d -M intel --start-address=0x00432800 --stop-address=0x00432a20 -- "$exe"
  printf '\n=== Timing and dispatch context 0x004360e0-0x00436190 ===\n'
  objdump -d -M intel --start-address=0x004360e0 --stop-address=0x00436190 -- "$exe"
  printf '\n=== Direct call sites referring to suspected functions ===\n'
  # Static call-site inventory is an index, not proof the call runs at Go.
  objdump -d -M intel -- "$exe" | grep -Ei 'call[[:space:]]+0x(4495c0|436100|432b00|427d00|427d24|432870)([[:space:]]|$)' || true
  printf '\nAddresses observed in Wine near image base 0x00400000.\n'
  printf 'Do not patch before confirming what these instructions do.\n'
} > "$report"
printf 'Rapport genere : %s\n' "$report"
