param(
    [Parameter(Mandatory)][string]$ScratchRoot,
    [string]$Compiler = 'C:/Program Files (x86)/Inno Setup 6/ISCC.exe'
)
$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
$root=Join-Path (Resolve-Path -LiteralPath $ScratchRoot).Path ('process-tests-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $root > $null
$env:TEMP=$root
$env:TMP=$root
$brand=Get-Content (Join-Path $repo 'branding.json') -Raw | ConvertFrom-Json
$installed=Join-Path $root 'installed app'
$other=Join-Path $root 'unrelated app'
New-Item -ItemType Directory -Path $installed,$other > $null
$fixture=Join-Path $installed "$($brand.name)_pp.exe"
$source=Join-Path $root 'fixture.cs'
@'
using System;
using System.Diagnostics;
using System.IO;
using System.Threading;
class Fixture {
    static void Main(string[] args) {
        if(args[0]=="graceful" || args[0]=="busy") {
            using(var signal=new EventWaitHandle(false,EventResetMode.AutoReset,args[2]+Process.GetCurrentProcess().Id)) {
                File.WriteAllText(args[1],"ready"); signal.WaitOne();
                if(args[0]=="busy") Thread.Sleep(Timeout.Infinite);
            }
        } else { File.WriteAllText(args[1],"ready"); Thread.Sleep(Timeout.Infinite); }
    }
}
'@ | Set-Content -LiteralPath $source -Encoding utf8
$csc=Join-Path $env:SystemRoot 'Microsoft.NET/Framework64/v4.0.30319/csc.exe'
& $csc /nologo /target:winexe "/out:$fixture" $source
if($LASTEXITCODE -ne 0){throw 'Fixture compilation failed'}
$otherExe=Join-Path $other (Split-Path $fixture -Leaf)
Copy-Item $fixture $otherExe
$production=Get-Content (Join-Path $repo 'installer/windows.iss') -Raw -Encoding utf8
$messages=($production -split '\[CustomMessages\]\r?\n')[1] -split '\[Tasks\]'
$messages=($messages[0] -split '\r?\n' | Where-Object {$_ -like 'english.*'}) -join "`n"
# Compile the production shutdown logic, substituting only the consent dialog.
$logic=($production -split '\[Code\]\r?\n')[1] -split 'function NextButtonClick'
$logic=$logic[0].Replace('MsgBox(', 'TestMsgBox(')
$harness=@"
#define AppName "$($brand.display_name)"
#define AppNameZh "$($brand.display_name_zh)"
#define AppPrefix "$($brand.name)"
#define AppExeName "$($brand.name)_pp.exe"
[Setup]
AppName=Installer Process Tests
AppVersion=1.0.0
DefaultDirName=$installed
OutputDir=$root
OutputBaseFilename=process-harness
Uninstallable=no
CreateAppDir=yes
UsePreviousAppDir=no
PrivilegesRequired=lowest
DisableWelcomePage=yes
DisableDirPage=yes
DisableReadyPage=yes
CloseApplications=no
[Languages]
Name: english; MessagesFile: compiler:Default.isl
[CustomMessages]
$messages
[Code]
var Prompts: Integer;
function TestMsgBox(Text: String; Kind: TMsgBoxType; Buttons: Integer): Integer;
begin
  Prompts := Prompts + 1;
  Result := IDYES;
  if (ExpandConstant('{param:CASE}') = 'decline') or
     ((ExpandConstant('{param:CASE}') = 'decline-force') and (Prompts = 2)) then Result := IDNO;
end;
$logic
function PrepareToInstall(var NeedsRestart: Boolean): String;
var Error, Scenario: String; ExpectedPrompts: Integer; ShouldRun: Boolean;
begin
  Scenario := ExpandConstant('{param:CASE}');
  Error := EnsureInstalledApplicationClosed(Scenario <> 'silent');
  ExpectedPrompts := 0;
  ShouldRun := (Scenario = 'decline') or (Scenario = 'decline-force') or (Scenario = 'silent');
  if (Scenario = 'decline') or (Scenario = 'graceful') then ExpectedPrompts := 1;
  if (Scenario = 'decline-force') or (Scenario = 'force') or (Scenario = 'timeout') then ExpectedPrompts := 2;
  if (Prompts <> ExpectedPrompts) or (InstalledExecutableIsRunning <> ShouldRun) or
     ((Error <> '') <> ShouldRun) then begin
    Result := Format('Scenario failed: %s, prompts=%d, error=%s', [Scenario, Prompts, Error]);
    Exit;
  end;
  SaveStringToFile(ExpandConstant('{param:REPORT}'), 'PASS ' + Scenario, False);
  Result := '';
end;
"@
$iss=Join-Path $root 'process-harness.iss'
Set-Content -LiteralPath $iss -Value $harness -Encoding utf8
& $Compiler /Q $iss
if($LASTEXITCODE -ne 0){throw 'Harness compilation failed'}
function Start-Fixture([string]$Exe,[string]$Mode,[string]$Marker) {
    $process=Start-Process -FilePath $Exe -ArgumentList @($Mode,"`"$Marker`"","`"Local\$($brand.name).InstallerExit.`"") -WindowStyle Hidden -PassThru
    for($i=0;$i -lt 40 -and -not (Test-Path $Marker);$i++){Start-Sleep -Milliseconds 100}
    if(-not (Test-Path $Marker)){throw 'Fixture did not become ready'}
    return $process
}
function Stop-Fixture($Process,[string]$Exe) {
    $live=Get-Process -Id $Process.Id -ErrorAction SilentlyContinue
    if($live -and $live.Path -ieq $Exe){Stop-Process -Id $live.Id; $live.WaitForExit()}
}
$unrelated=Start-Fixture $otherExe 'legacy' (Join-Path $root 'other-ready')
try {
    foreach($scenario in 'none','decline','decline-force','silent','force','graceful','timeout') {
        $process=$null
        try {
            if($scenario -ne 'none'){
                $mode=if($scenario -eq 'graceful'){'graceful'}elseif($scenario -eq 'timeout'){'busy'}else{'legacy'}
                $process=Start-Fixture $fixture $mode (Join-Path $root "$scenario-ready")
            }
            $report=Join-Path $root "$scenario-result.txt"
            $setup=Start-Process -FilePath (Join-Path $root 'process-harness.exe') -ArgumentList @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART',"/CASE=$scenario","/REPORT=`"$report`"","/LOG=`"$root\$scenario.log`"") -WindowStyle Hidden -PassThru
            if(-not $setup.WaitForExit(30000)){Stop-Fixture $setup (Join-Path $root 'process-harness.exe');throw "Harness timeout: $scenario"}
            if($setup.ExitCode -ne 0 -or -not (Test-Path $report)){throw "Harness failed: $scenario (exit $($setup.ExitCode)), see $root"}
            if(-not (Get-Process -Id $unrelated.Id -ErrorAction SilentlyContinue)){throw 'Unrelated same-name process was stopped'}
            Get-Content $report
        } finally {if($process){Stop-Fixture $process $fixture}}
    }
} finally {Stop-Fixture $unrelated $otherExe}
Write-Host "Installer process tests passed: $root"
