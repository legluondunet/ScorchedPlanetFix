#requires -Version 5.1
<#
.SYNOPSIS
  Collect read-only process evidence of a Scorched Planet 3dfx startup hang.
.DESCRIPTION
  Start SPVOODOO.EXE, wait for the user to report a frozen mission transition,
  and take two CPU/thread/module snapshots. Original game files are not patched.
  Optional full dumps can contain private data and must not be committed.
.EXAMPLE
  powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\diagnose-3dfx-hang.ps1 -GameDir 'C:\Games\ScorchedPlanet'
.EXAMPLE
  powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\diagnose-3dfx-hang.ps1 -GameDir 'C:\Games\ScorchedPlanet' -SingleCore -CaptureDumps
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string] $GameDir,

    [switch] $SingleCore,

    [switch] $CaptureDumps,

    [string] $ProcDumpPath,

    [ValidateRange(2, 60)]
    [int] $IntervalSeconds = 5,

    [string] $OutputDir
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$GameDir = (Resolve-Path -LiteralPath $GameDir -ErrorAction Stop).ProviderPath
$exe = Join-Path $GameDir 'SPVOODOO.EXE'
if (-not (Test-Path -LiteralPath $exe -PathType Leaf)) {
    throw "SPVOODOO.EXE introuvable dans '$GameDir'."
}

if ([string]::IsNullOrWhiteSpace($OutputDir)) {
    $root = if ($env:LOCALAPPDATA) { $env:LOCALAPPDATA } else { $env:TEMP }
    $OutputDir = Join-Path $root ("ScorchedPlanetFix\Diagnostics\" + (Get-Date -Format 'yyyyMMdd-HHmmss'))
}
New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
$OutputDir = (Resolve-Path -LiteralPath $OutputDir).ProviderPath
$logPath = Join-Path $OutputDir 'summary.txt'

function Write-Report([string] $Message) {
    $line = ('[{0}] {1}' -f (Get-Date -Format 'yyyy-MM-dd HH:mm:ss.fff'), $Message)
    $line | Out-File -LiteralPath $logPath -Encoding utf8 -Append
    Write-Host $line
}

Write-Report "Scorched Planet — diagnostic de blocage 3dfx (fichiers originaux intacts)"
Write-Report ("Windows PowerShell : " + $PSVersionTable.PSVersion.ToString())
Write-Report "SingleCore=$SingleCore CaptureDumps=$CaptureDumps IntervalSeconds=$IntervalSeconds"
Write-Report "Attention : les rapports contiennent des chemins locaux ; les dumps peuvent contenir des donnees privees."

$hashes = @(
    Get-ChildItem -LiteralPath $GameDir -File | Where-Object {
        $_.Name -match '^(SPVOODOO\.EXE|SPLANET\.EXE|RWDLVOOD\.DLL|GLIDE2X?\.DLL|DDRAW\.DLL|WINMM\.DLL|DSOUND\.DLL)$'
    } | ForEach-Object {
        try {
            $digest = Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256
            [pscustomobject]@{
                File = $_.Name
                SizeBytes = $_.Length
                SHA256 = $digest.Hash
            }
        } catch {
            Write-Report ("Hash impossible pour " + $_.Name + ": " + $_.Exception.Message)
        }
    }
)
$hashes | Export-Csv -LiteralPath (Join-Path $OutputDir 'game-files.csv') -NoTypeInformation -Encoding UTF8

if ($CaptureDumps) {
    if (-not [string]::IsNullOrWhiteSpace($ProcDumpPath)) {
        if (-not (Test-Path -LiteralPath $ProcDumpPath -PathType Leaf)) {
            throw "ProcDump introuvable : '$ProcDumpPath'."
        }
        $dumpTool = (Resolve-Path -LiteralPath $ProcDumpPath).ProviderPath
    } else {
        $command = Get-Command -Name 'procdump64.exe', 'procdump.exe' -ErrorAction SilentlyContinue | Select-Object -First 1
        $dumpTool = if ($null -ne $command) { $command.Source } else { $null }
    }
    if ($null -eq $dumpTool) {
        Write-Report "ProcDump introuvable : seuls les snapshots texte seront produits."
        $CaptureDumps = $false
    }
}

function Save-Snapshot([int] $Index, [int] $PidToInspect) {
    $prefix = ('snapshot-{0}' -f $Index)
    $proc = Get-Process -Id $PidToInspect -ErrorAction Stop
    $proc.Refresh()
    Write-Report ("$prefix PID=$($proc.Id) TotalCPUSeconds=$($proc.TotalProcessorTime.TotalSeconds) " +
        "WorkingSetBytes=$($proc.WorkingSet64) Threads=$($proc.Threads.Count)")

    $threadRows = @(
        foreach ($thread in $proc.Threads) {
            try {
                $waitReason = ''
                if ($thread.ThreadState -eq [System.Diagnostics.ThreadState]::Wait) {
                    $waitReason = $thread.WaitReason.ToString()
                }
                [pscustomobject]@{
                    ThreadId = $thread.Id
                    State = $thread.ThreadState.ToString()
                    WaitReason = $waitReason
                    CPUms = [math]::Round($thread.TotalProcessorTime.TotalMilliseconds, 2)
                    UserCPUms = [math]::Round($thread.UserProcessorTime.TotalMilliseconds, 2)
                }
            } catch {
                Write-Report ("Impossible de lire un thread : " + $_.Exception.Message)
            }
        }
    )
    $threadRows | Export-Csv -LiteralPath (Join-Path $OutputDir "$prefix-threads.csv") -NoTypeInformation -Encoding UTF8

    try {
        $modules = @(
            foreach ($module in $proc.Modules) {
                [pscustomobject]@{
                    Name = $module.ModuleName
                    Path = $module.FileName
                    BaseAddress = ('0x{0:X}' -f $module.BaseAddress.ToInt64())
                    ModuleMemorySize = $module.ModuleMemorySize
                }
            }
        )
        $modules | Export-Csv -LiteralPath (Join-Path $OutputDir "$prefix-modules.csv") -NoTypeInformation -Encoding UTF8
        Write-Report ("$prefix : " + $modules.Count + " modules releves")
    } catch {
        Write-Report ("Impossible de lire les modules ($prefix) : " + $_.Exception.Message)
    }

    if ($CaptureDumps) {
        $dumpFile = Join-Path $OutputDir "$prefix.dmp"
        Write-Report "Capture d'un dump memoire (peut etre volumineux)."
        & $dumpTool -accepteula -ma $PidToInspect $dumpFile 2>&1 | Out-File -LiteralPath (Join-Path $OutputDir "$prefix-procdump.txt") -Encoding utf8
        Write-Report ("ProcDump exit code : " + $LASTEXITCODE)
    }
}

Write-Report "Lancement de SPVOODOO.EXE..."
$game = Start-Process -FilePath $exe -WorkingDirectory $GameDir -PassThru
Write-Report ("PID du jeu : " + $game.Id)

if ($SingleCore) {
    try {
        $game.ProcessorAffinity = [IntPtr] 1
        Write-Report "Affinite limitee au processeur logique 0 pour ce test uniquement."
    } catch {
        Write-Report ("Impossible d'appliquer l'affinite : " + $_.Exception.Message)
    }
}

Write-Host ""
Write-Host "Dans Scorched Planet, demarrez une mission." -ForegroundColor Cyan
Write-Host "Quand le menu est gele, revenez a PowerShell et appuyez sur Entree." -ForegroundColor Cyan
Read-Host "Appuyez sur Entree pendant le blocage" | Out-Null

$game.Refresh()
if ($game.HasExited) {
    Write-Report ("Le processus est deja termine. Code de sortie : " + $game.ExitCode)
} else {
    try {
        Save-Snapshot -Index 1 -PidToInspect $game.Id
        Write-Report ("Intervalle de " + $IntervalSeconds + " secondes avant la seconde capture.")
        Start-Sleep -Seconds $IntervalSeconds
        $game.Refresh()
        if ($game.HasExited) {
            Write-Report "Le jeu s'est termine entre les deux captures."
        } else {
            Save-Snapshot -Index 2 -PidToInspect $game.Id
        }
    } catch {
        Write-Report ("Erreur pendant la collecte : " + $_.Exception.Message)
    }
}

Write-Report ("Fin. Rapport : " + $OutputDir)
Write-Host "A partager : summary.txt, game-files.csv, snapshot-*-threads.csv et snapshot-*-modules.csv." -ForegroundColor Green
Write-Host "Ne publiez pas les .dmp : ils peuvent contenir des donnees privees." -ForegroundColor Yellow
Write-Host "Le script ne termine pas le jeu ; fermez-le manuellement si necessaire."
