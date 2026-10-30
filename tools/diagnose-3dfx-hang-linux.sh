#!/usr/bin/env bash
# Read-only diagnostic for Scorched Planet 3dfx running under Linux/Wine.
set -Eeuo pipefail
export LC_ALL=C

usage() {
  cat <<'EOF'
Usage: diagnose-3dfx-hang-linux.sh [--pid LINUX_PID] [--game-dir PATH]
          [--interval SECONDS] [--delay SECONDS] [--wait] [--output NEW_DIR]
          [--gdb | --gdb-sudo]

By default, launch SPVOODOO.EXE in Wine first, then run this script.
With --wait, start the script BEFORE the game, then launch the game normally.
This avoids using Alt+Tab once the game enters fullscreen.
Optional WINEPREFIX environment variable identifies the intended Wine prefix.
The tool compares it with the process WINEPREFIX when available; no Wine launch.
--pid: specify Linux PID if spvoodoo.exe is not detected automatically
WINEPREFIX=/path/to/prefix: optional process matching and logging
--game-dir: optionally hash the game's original EXE and local Glide DLLs
--interval: seconds between snapshots (default 5, range 1-60)
--delay: seconds after process detection before the first snapshot (0-600)
--wait: wait up to 120 seconds for the game to launch (avoids Alt+Tab)
--output: a NEW, non-existing directory for detailed reports
Default: a dated report directory plus a .log in your current directory.
--gdb: optionally take two GDB samples of thread stacks and instructions;
       briefly pauses the game and might fail due to ptrace policy
--gdb-sudo: opt-in privileged GDB attach using sudo (without changing
            kernel-wide ptrace_scope). Asks sudo authorization BEFORE launch.
EOF
}

die() { printf 'Erreur : %s\n' "$*" >&2; exit 1; }
log() {
  printf '[%s] %s\n' "$(date '+%Y-%m-%d %H:%M:%S')" "$*" |
    tee -a "$out/summary.txt" "$log_file"
}

pid=""
game_dir=""
interval=5
delay=0
out=""
with_gdb=0
use_sudo_gdb=0
wait_for_game=0
expected_prefix=""
observed_prefix=""

# The user's WINEPREFIX is optional. It is a selector, not a command to
# launch a second Wine instance. Do not bake any private machine paths in.
if [[ -n "${WINEPREFIX:-}" ]]; then
  [[ -d "$WINEPREFIX" ]] || die "Le WINEPREFIX indique n'existe pas : $WINEPREFIX"
  expected_prefix="$(realpath -- "$WINEPREFIX")"
fi

# Read just the WINEPREFIX entry from a process environment (NUL-delimited).
# We intentionally do not log or store other environment entries.
read_process_prefix() {
  local item
  [[ -r "/proc/$1/environ" ]] || return 1
  while IFS= read -r -d '' item; do
    case "$item" in
      WINEPREFIX=*) printf '%s\n' "${item#WINEPREFIX=}"; return 0 ;;
    esac
  done < "/proc/$1/environ"
  return 1
}

while (($# > 0)); do
  case "$1" in
    --pid|--game-dir|--interval|--delay|--output)
      opt="$1"; shift
      (($# > 0)) || die "Argument manquant pour $opt"
      case "$opt" in
        --pid) pid="$1" ;;
        --game-dir) game_dir="$1" ;;
        --interval) interval="$1" ;;
        --delay) delay="$1" ;;
        --output) out="$1" ;;
      esac
      ;;
    --gdb) with_gdb=1 ;;
    --gdb-sudo) with_gdb=1; use_sudo_gdb=1 ;;
    --wait) wait_for_game=1 ;;
    -h|--help) usage; exit 0 ;;
    *) usage >&2; die "Option inconnue : $1" ;;
  esac
  shift
done

[[ "$interval" =~ ^[0-9]+$ ]] && ((interval >= 1 && interval <= 60)) ||
  die "L'intervalle doit etre compris entre 1 et 60 secondes."
[[ "$delay" =~ ^[0-9]+$ ]] && ((delay <= 600)) ||
  die "Le delai doit etre compris entre 0 et 600 secondes."

# Ask for administrative authorization while the terminal is still in focus,
# before launching fullscreen Wine. Never change system-wide ptrace policy.
if ((use_sudo_gdb)); then
  command -v gdb >/dev/null 2>&1 || die "GDB absent (requis pour --gdb-sudo)"
  command -v sudo >/dev/null 2>&1 || die "sudo absent (requis pour --gdb-sudo)"
  printf "Mode GDB privilegie : sudo autorisera un attachement au processus du jeu.\n" >&2
  printf "Aucun changement de kernel.yama.ptrace_scope ne sera effectue.\n" >&2
  sudo -v || die "Autorisation sudo refusee; relance sans --gdb-sudo"
fi

if [[ -z "$pid" ]]; then
  if ((wait_for_game)); then
    printf 'En attente de SPVOODOO.EXE (max 120 s). Lance le jeu maintenant, sans Alt+Tab ulterieur...\n'
  fi
  candidates=()
  for ((attempt=0; attempt<=120; attempt++)); do
    mapfile -t candidates < <(ps -eo pid=,comm= | awk 'tolower($2) == "spvoodoo.exe" {print $1}')
    if [[ -n "$expected_prefix" ]]; then
      eligible=()
      unknown=()
      for candidate in "${candidates[@]}"; do
        seen="$(read_process_prefix "$candidate" 2>/dev/null || true)"
        if [[ -z "$seen" ]]; then
          unknown+=("$candidate")
        elif [[ "$(realpath -m -- "$seen")" == "$expected_prefix" ]]; then
          eligible+=("$candidate")
        fi
      done
      # Exact matches take priority. Do not silently accept a known mismatch.
      if (("${#eligible[@]}" > 0)); then
        candidates=("${eligible[@]}")
      else
        candidates=("${unknown[@]}")
      fi
    fi
    if (("${#candidates[@]}" > 0)) || (( !wait_for_game || attempt == 120 )); then
      break
    fi
    sleep 1
  done
  case "${#candidates[@]}" in
    0) die "Jeu non detecte. Lance SPVOODOO.EXE, ou precise --pid (ps -eo pid,comm,args | grep -i '[s]pvoodoo')." ;;
    1) pid="${candidates[0]}" ;;
    *) printf 'PIDs possibles : %s\n' "${candidates[*]}" >&2
       die "Plusieurs instances, precise --pid." ;;
  esac
fi

[[ "$pid" =~ ^[0-9]+$ ]] || die "PID Linux invalide : $pid"
[[ -d "/proc/$pid/task" ]] || die "Processus $pid introuvable"
kill -0 "$pid" 2>/dev/null || die "Pas d'acces au processus $pid"

if [[ -n "$expected_prefix" ]]; then
  observed_prefix="$(read_process_prefix "$pid" 2>/dev/null || true)"
  if [[ -n "$observed_prefix" ]]; then
    [[ "$(realpath -m -- "$observed_prefix")" == "$expected_prefix" ]] ||
      die "Le PID $pid appartient a un autre prefixe Wine : $observed_prefix"
  fi
fi

if [[ -n "$game_dir" ]]; then
  [[ -d "$game_dir" ]] || die "Dossier du jeu introuvable : $game_dir"
  game_dir="$(realpath -- "$game_dir")"
fi

# Save a visible, timestamped log directly in the directory from which this
# script is run (normally the game directory). Keep auxiliary files grouped.
run_id="$(date +%Y%m%d-%H%M%S)-pid${pid}-shell${BASHPID}"
log_file="$PWD/ScorchedPlanetFix-3dfx-$run_id.log"
if [[ -z "$out" ]]; then
  out="$PWD/ScorchedPlanetFix-3dfx-$run_id"
fi
[[ ! -e "$out" ]] || die "Le dossier existe deja : $out"
mkdir -p -- "$out"
out="$(realpath -- "$out")"
[[ ! -e "$log_file" ]] || die "Le fichier de log existe deja : $log_file"
log "Diagnostic Scorched Planet / Wine / nGlide"
log "Fichier de log : $log_file"
log "Linux PID=$pid interval=${interval}s delay=${delay}s GDB=$with_gdb GDB_SUDO=$use_sudo_gdb"
if [[ -n "$expected_prefix" ]]; then
  log "WINEPREFIX demande=$expected_prefix"
  if [[ -n "$observed_prefix" ]]; then
    log "WINEPREFIX processus confirme=$(realpath -m -- "$observed_prefix")"
  else
    log "WINEPREFIX processus non lisible ou non exporte : appartenance non verifiee"
  fi
fi
log "Les fichiers du jeu et le prefixe Wine ne seront pas modifies."
log "Les rapports contiennent des chemins locaux : relis-les avant de les partager."
log "CLK_TCK=$(getconf CLK_TCK 2>/dev/null || printf '?')"
if [[ -r /proc/sys/kernel/yama/ptrace_scope ]]; then
  log "Yama ptrace_scope=$(cat /proc/sys/kernel/yama/ptrace_scope) (lecture seule)"
else
  log "Yama ptrace_scope indisponible"
fi
if [[ -r "/proc/$pid/status" ]]; then
  log "TracerPid=$(awk '/^TracerPid:/ {print $2}' "/proc/$pid/status")"
fi
log "Session graphique : XDG_SESSION_TYPE=${XDG_SESSION_TYPE:-unknown} DISPLAY=${DISPLAY:-unset} WAYLAND_DISPLAY=${WAYLAND_DISPLAY:-unset}"
if command -v wine >/dev/null 2>&1; then
  log "Wine dans le PATH (pas necessairement celui du processus)=$(wine --version 2>/dev/null || printf '?')"
fi
if [[ -e "/proc/$pid/exe" ]]; then
  log "Binaire Linux reel du processus=$(readlink "/proc/$pid/exe" 2>/dev/null || printf '?')"
fi

ps -p "$pid" -o pid=,ppid=,comm=,args= > "$out/process.txt" || true
if [[ -n "$game_dir" ]]; then
  find "$game_dir" -maxdepth 1 -type f \( \
    -iname 'SPVOODOO.EXE' -o -iname 'RWDLVOOD.DLL' -o \
    -iname 'GLIDE.DLL' -o -iname 'GLIDE2X.DLL' -o \
    -iname 'DDRAW.DLL' -o -iname 'WINMM.DLL' \) -print0 |
    while IFS= read -r -d '' file; do sha256sum -- "$file" || true; done \
    > "$out/game-file-hashes.txt"
fi

snapshot() {
  local name="$1" stat_file stat_line remainder tid ticks wchan
  local -a fields
  [[ -d "/proc/$pid/task" ]] || { log "Jeu termine avant $name"; return 1; }
  log "Capture $name"
  ps -L -p "$pid" -o pid,tid,stat,psr,pcpu,time,wchan:30,comm \
    > "$out/$name-threads-ps.txt" || true
  ps -p "$pid" -o pid,ppid,stat,pcpu,rss,vsz,etime,comm \
    > "$out/$name-process-ps.txt" || true
  # Query display mode without changing it, to investigate the unexpected
  # fullscreen -> 640x480 transition observed after Alt+Tab.
  if command -v xrandr >/dev/null 2>&1 && [[ -n "${DISPLAY:-}" ]]; then
    timeout 3 xrandr --current > "$out/$name-xrandr.txt" 2>&1 || true
  fi
  {
    printf 'TID\tCPU_TICKS\tSTATE\tWCHAN\n'
    for stat_file in /proc/"$pid"/task/*/stat; do
      [[ -r "$stat_file" ]] || continue
      tid="$(basename "$(dirname "$stat_file")")"
      IFS= read -r stat_line < "$stat_file" || continue
      remainder="${stat_line##*) }"
      read -r -a fields <<< "$remainder"
      (("${#fields[@]}" >= 13)) || continue
      ticks=$(( fields[11] + fields[12] ))
      wchan="$(cat "/proc/$pid/task/$tid/wchan" 2>/dev/null || printf '?')"
      printf '%s\t%s\t%s\t%s\n' "$tid" "$ticks" "${fields[0]}" "$wchan"
    done
  } > "$out/$name-thread-ticks.tsv"
  if [[ -r "/proc/$pid/maps" ]]; then
    awk '$6 ~ /^\// {
      p=tolower($6)
      if (p ~ /(rwdlvood|spvoodoo|glide|nglide|ddraw|opengl|vulkan|d3d|dxvk|wine)/) print $6
    }' "/proc/$pid/maps" | sort -u > "$out/$name-graphics-modules.txt"
  else
    log "Impossible de lire /proc/$pid/maps (permissions)."
  fi
}

if ((delay > 0)); then
  log "La premiere capture commence dans $delay secondes : lance une mission."
  sleep "$delay"
else
  log "Capture immediate : le jeu devrait deja etre fige."
fi
snapshot snapshot-1 || exit 1
sleep "$interval"
snapshot snapshot-2 || exit 0

{
  printf 'TID\tCPU_DELTA_TICKS\n'
  awk -F '\t' '
    NR==FNR { if (NR>1 && $1 ~ /^[0-9]+$/) before[$1]=$2; next }
    FNR>1 && $1 ~ /^[0-9]+$/ {
      if ($1 in before) printf "%s\t%d\n", $1, $2-before[$1]
      else printf "%s\tnew-thread\n", $1
    }' "$out/snapshot-1-thread-ticks.tsv" "$out/snapshot-2-thread-ticks.tsv" |
    sort -k2,2nr
} > "$out/thread-cpu-delta.tsv"

if ((with_gdb)); then
  if ! command -v gdb >/dev/null 2>&1; then
    log "GDB absent : capture des piles non effectuee."
  else
    # Native GDB must attach to the Linux PID, not the Windows PID.
    # An optional, explicitly selected sudo attach avoids changing the
    # system-wide Yama ptrace_scope. sudo -n never prompts during fullscreen.
    debugger=(gdb)
    if ((use_sudo_gdb)); then debugger=(sudo -n gdb); fi
    for sample in 1 2; do
      [[ -d "/proc/$pid/task" ]] || { log "Jeu termine avant GDB capture $sample/2"; break; }
      log "GDB capture $sample/2 : attachement temporaire, peut suspendre le jeu."
      if "${debugger[@]}" -q -nx -batch \
        -iex 'set auto-load off' \
        -p "$pid" \
        -ex 'set confirm off' \
        -ex 'set pagination off' \
        -ex 'set print thread-events off' \
        -ex 'info threads' \
        -ex 'info proc mappings' \
        -ex 'thread apply all p/x $pc' \
        -ex 'thread apply all x/6i $pc' \
        -ex 'thread apply all bt 12' \
        -ex 'detach' \
        > "$out/gdb-sample-$sample.txt" 2>&1; then
        log "GDB capture $sample/2 reussie."
      else
        log "GDB capture $sample/2 impossible : voir le rapport detaille."
        if grep -qiE 'ptrace: Operation not permitted|Operation not permitted|sudo: a password is required' "$out/gdb-sample-$sample.txt"; then
          log "Permissions ptrace refusees : relancer avec --gdb-sudo si vous autorisez une capture privilegiee."
          break
        fi
      fi
      if [[ ! -d "/proc/$pid/task" ]]; then
        log "Processus Wine disparu apres GDB capture $sample/2 (cause non determinee)."
        break
      fi
      if (( sample == 1 )); then sleep 3; fi
    done
    if [[ -f "$out/gdb-sample-1.txt" ]]; then
      {
        printf '\n=== GDB : thread principal Linux TID %s ===\n' "$pid"
        for sample in 1 2; do
          file="$out/gdb-sample-$sample.txt"
          [[ -f "$file" ]] || continue
          printf '\n=== GDB capture %s : PC, instructions et pile du TID %s ===\n' "$sample" "$pid"
          # GDB emits one "Thread N (... (LWP PID)):" header per thread
          # for each "thread apply all" command. Extract only the main
          # Linux thread and the first 20 following lines per section.
          # Do not flood the single log with every Wine helper thread.
          awk -v tid="$pid" '
            /^Thread [0-9]+ / {
              active = (index($0, "LWP " tid ")") > 0)
              if (active) { section++; lines=0; print "\n" $0 }
              next
            }
            active && lines < 20 {
              print
              lines++
            }
          ' "$file"
          if ! grep -q "LWP $pid)" "$file"; then
            printf 'Aucune section LWP %s identifiable; voir %s\n' "$pid" "$file"
            grep -E -m 6 'ptrace:|Operation not permitted|Could not attach|warning:' "$file" || true
          fi
        done
        printf '\nPiles de tous les threads : dans gdb-sample-1.txt et gdb-sample-2.txt.\n'
      } >> "$log_file"
    fi
  fi
fi
log "Rapport cree : $out"
if [[ -s "$out/thread-cpu-delta.tsv" ]]; then
  {
    printf '\n=== Evolution CPU par thread (ticks sur %s s) ===\n' "$interval"
    cat "$out/thread-cpu-delta.tsv"
    printf '\n=== Modules graphiques detectes ===\n'
    cat "$out/snapshot-2-graphics-modules.txt" 2>/dev/null || true
  } >> "$log_file"
fi
log "Le fichier .log se trouve directement dans le dossier de lancement."
log "Partager le .log et eventuellement les fichiers detail (relire les chemins prives)."
