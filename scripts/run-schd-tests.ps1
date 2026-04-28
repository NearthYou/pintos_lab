param(
    [ValidateSet("schd1", "schd2", "schd12", "schd3", "mlfqs")]
    [string]$Stage = "schd12",

    [string]$ContainerName = "",

    [string]$PintosRoot = "",

    [switch]$NoClean
)

$ErrorActionPreference = "Stop"

function Get-PintosContainer {
    if ($ContainerName) {
        return $ContainerName
    }

    $containers = @(docker ps --format "{{.Names}}" | Where-Object {
        $_ -match "pintos|dev|jungle"
    })

    if ($containers.Count -eq 1) {
        return $containers[0]
    }

    if ($containers.Count -gt 1) {
        Write-Host "Multiple matching containers found. Pass one with -ContainerName:"
        $containers | ForEach-Object { Write-Host "  $_" }
        exit 1
    }

    Write-Host "No running Pintos container was found."
    Write-Host "Start the Dev Container or Docker container first."
    Write-Host "Example: .\scripts\run-schd-tests.ps1 -Stage schd1 -ContainerName <name>"
    exit 1
}

$targetsByStage = @{
    schd1 = @(
        "build/tests/threads/alarm-priority.result",
        "build/tests/threads/priority-preempt.result",
        "build/tests/threads/priority-change.result",
        "build/tests/threads/priority-fifo.result"
    )
    schd2 = @(
        "build/tests/threads/priority-sema.result",
        "build/tests/threads/priority-condvar.result"
    )
    schd12 = @(
        "build/tests/threads/alarm-priority.result",
        "build/tests/threads/priority-preempt.result",
        "build/tests/threads/priority-change.result",
        "build/tests/threads/priority-fifo.result",
        "build/tests/threads/priority-sema.result",
        "build/tests/threads/priority-condvar.result"
    )
    schd3 = @(
        "build/tests/threads/priority-donate-one.result",
        "build/tests/threads/priority-donate-multiple.result",
        "build/tests/threads/priority-donate-multiple2.result",
        "build/tests/threads/priority-donate-nest.result",
        "build/tests/threads/priority-donate-sema.result",
        "build/tests/threads/priority-donate-lower.result",
        "build/tests/threads/priority-donate-chain.result"
    )
    mlfqs = @(
        "build/tests/threads/mlfqs/mlfqs-load-1.result",
        "build/tests/threads/mlfqs/mlfqs-load-60.result",
        "build/tests/threads/mlfqs/mlfqs-load-avg.result",
        "build/tests/threads/mlfqs/mlfqs-recent-1.result",
        "build/tests/threads/mlfqs/mlfqs-fair-2.result",
        "build/tests/threads/mlfqs/mlfqs-fair-20.result",
        "build/tests/threads/mlfqs/mlfqs-nice-2.result",
        "build/tests/threads/mlfqs/mlfqs-nice-10.result",
        "build/tests/threads/mlfqs/mlfqs-block.result"
    )
}

$container = Get-PintosContainer
$targets = $targetsByStage[$Stage]
$targetArgs = $targets -join " "
$cleanCommand = if ($NoClean) { "" } else { "make -C `"`$PINTOS_ROOT/threads`" clean" }
$rootExport = if ($PintosRoot) { "PINTOS_ROOT='$PintosRoot'" } else { "PINTOS_ROOT=''" }

$bashScript = @"
set -e
$rootExport

if [ -z "`$PINTOS_ROOT" ]; then
  for candidate in /workspace/pintos /workspaces/*/pintos /home/jungle/pintos; do
    if [ -f "`$candidate/activate" ]; then
      PINTOS_ROOT="`$candidate"
      break
    fi
  done
fi

if [ -z "`$PINTOS_ROOT" ] || [ ! -f "`$PINTOS_ROOT/activate" ]; then
  echo "Could not find PINTOS_ROOT. Pass -PintosRoot /path/to/pintos."
  exit 1
fi

export PINTOS_ROOT
source "`$PINTOS_ROOT/activate"
echo "PINTOS_ROOT=`$PINTOS_ROOT"
echo "STAGE=$Stage"

$cleanCommand
make -C "`$PINTOS_ROOT/threads" $targetArgs

echo
echo "===== results ====="
for result in $targetArgs; do
  path="`$PINTOS_ROOT/threads/`$result"
  echo "----- `$result -----"
  if [ -f "`$path" ]; then
    cat "`$path"
  else
    echo "missing: `$path"
  fi
done
"@

docker exec -i $container bash -lc $bashScript
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}
