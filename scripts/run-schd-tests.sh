#!/usr/bin/env bash
set -euo pipefail

stage="${1:-schd12}"
pintos_root="${PINTOS_ROOT:-}"
clean=1

while [[ $# -gt 0 ]]; do
  case "$1" in
    schd1|schd2|schd12|schd3|mlfqs)
      stage="$1"
      shift
      ;;
    --pintos-root)
      pintos_root="$2"
      shift 2
      ;;
    --no-clean)
      clean=0
      shift
      ;;
    -h|--help)
      echo "Usage: $0 [schd1|schd2|schd12|schd3|mlfqs] [--pintos-root PATH] [--no-clean]"
      exit 0
      ;;
    *)
      echo "Unknown argument: $1" >&2
      echo "Usage: $0 [schd1|schd2|schd12|schd3|mlfqs] [--pintos-root PATH] [--no-clean]" >&2
      exit 1
      ;;
  esac
done

if [[ -z "$pintos_root" ]]; then
  for candidate in /workspace/pintos /workspaces/*/pintos /home/jungle/pintos; do
    if [[ -f "$candidate/activate" ]]; then
      pintos_root="$candidate"
      break
    fi
  done
fi

if [[ -z "$pintos_root" || ! -f "$pintos_root/activate" ]]; then
  echo "Could not find PINTOS_ROOT. Pass --pintos-root /path/to/pintos." >&2
  exit 1
fi

case "$stage" in
  schd1)
    targets=(
      build/tests/threads/alarm-priority.result
      build/tests/threads/priority-preempt.result
      build/tests/threads/priority-change.result
      build/tests/threads/priority-fifo.result
    )
    ;;
  schd2)
    targets=(
      build/tests/threads/priority-sema.result
      build/tests/threads/priority-condvar.result
    )
    ;;
  schd12)
    targets=(
      build/tests/threads/alarm-priority.result
      build/tests/threads/priority-preempt.result
      build/tests/threads/priority-change.result
      build/tests/threads/priority-fifo.result
      build/tests/threads/priority-sema.result
      build/tests/threads/priority-condvar.result
    )
    ;;
  schd3)
    targets=(
      build/tests/threads/priority-donate-one.result
      build/tests/threads/priority-donate-multiple.result
      build/tests/threads/priority-donate-multiple2.result
      build/tests/threads/priority-donate-nest.result
      build/tests/threads/priority-donate-sema.result
      build/tests/threads/priority-donate-lower.result
      build/tests/threads/priority-donate-chain.result
    )
    ;;
  mlfqs)
    targets=(
      build/tests/threads/mlfqs/mlfqs-load-1.result
      build/tests/threads/mlfqs/mlfqs-load-60.result
      build/tests/threads/mlfqs/mlfqs-load-avg.result
      build/tests/threads/mlfqs/mlfqs-recent-1.result
      build/tests/threads/mlfqs/mlfqs-fair-2.result
      build/tests/threads/mlfqs/mlfqs-fair-20.result
      build/tests/threads/mlfqs/mlfqs-nice-2.result
      build/tests/threads/mlfqs/mlfqs-nice-10.result
      build/tests/threads/mlfqs/mlfqs-block.result
    )
    ;;
esac

export PINTOS_ROOT="$pintos_root"
source "$PINTOS_ROOT/activate"

echo "PINTOS_ROOT=$PINTOS_ROOT"
echo "STAGE=$stage"

if [[ "$clean" -eq 1 ]]; then
  make -C "$PINTOS_ROOT/threads" clean
fi

make -C "$PINTOS_ROOT/threads" "${targets[@]}"

echo
echo "===== results ====="
for result in "${targets[@]}"; do
  path="$PINTOS_ROOT/threads/$result"
  echo "----- $result -----"
  if [[ -f "$path" ]]; then
    cat "$path"
  else
    echo "missing: $path"
  fi
done
