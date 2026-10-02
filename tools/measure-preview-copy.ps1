# Checks what a copy from the Markdown++ preview pane actually puts on the clipboard, and
# what Microsoft Word does with it.
#
# WHY THIS EXISTS
#
# Chromium's default copy inlines the COMPUTED style of every selected node. In the preview
# that means the dark theme travels with the text: measured on 2026-10-02, a pane copy pasted
# into Word as rgb(230, 237, 243), near-white on a white page. Bold and italic were present
# and invisible. preview.js now serialises the selection itself, and this script is what proves
# it still does. ctest cannot: preview.js has no JavaScript test runner in this repo, and the
# defect only exists once a real WebView2 and a real paste target are involved.
#
# REQUIREMENTS
#
#   - the plugin INSTALLED into Notepad++ (tools/install-native-plugin.ps1), not merely built
#   - Notepad++ closed, or at least willing to open another tab
#   - Microsoft Word, for the paste half. Without it the clipboard assertions still run and
#     the Word assertions are reported as NOT RUN, loudly, never skipped in silence.
#
# Run it under powershell.exe (5.1). System.Windows.Forms.Clipboard requires STA, and pwsh 7
# is MTA by default, where the clipboard calls fail.
#
#   powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\measure-preview-copy.ps1
#
# Exits 0 when every assertion that ran passed, 1 otherwise.

param(
    [string] $File,
    [string] $NotepadExe = 'C:\Program Files\Notepad++\notepad++.exe',
    [string] $PreviewClass = 'MarkdownPlusPlusNativePreview',
    [string] $Sentinel = 'MDPP-COPY-SENTINEL-DO-NOT-PASTE',
    [switch] $KeepNotepadOpen
)

$ErrorActionPreference = 'Stop'

if (-not $File) {
    $File = Join-Path (Split-Path -Parent $PSScriptRoot) 'smoke-tests\clipboard-copy.md'
}

# The preview's dark foreground. If this reaches the clipboard the defect is back.
$DarkThemeForeground = 'rgb(230, 237, 243)'

Add-Type -AssemblyName System.Windows.Forms

Add-Type -TypeDefinition @"
using System;
using System.Text;
using System.Runtime.InteropServices;

public class MdppCopyProbe
{
    public delegate bool EnumProc(IntPtr hwnd, IntPtr param);

    [DllImport("user32.dll")]
    public static extern bool EnumChildWindows(IntPtr parent, EnumProc callback, IntPtr param);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern int GetClassName(IntPtr hwnd, StringBuilder name, int count);

    [DllImport("user32.dll")]
    public static extern bool GetWindowRect(IntPtr hwnd, out RECT rect);

    [DllImport("user32.dll")]
    public static extern bool IsWindowVisible(IntPtr hwnd);

    [DllImport("user32.dll")]
    public static extern bool SetForegroundWindow(IntPtr hwnd);

    [DllImport("user32.dll")]
    public static extern bool SetCursorPos(int x, int y);

    [DllImport("user32.dll")]
    public static extern void mouse_event(uint flags, uint dx, uint dy, uint data, IntPtr extra);

    [StructLayout(LayoutKind.Sequential)]
    public struct RECT { public int Left; public int Top; public int Right; public int Bottom; }

    public const uint MOUSEEVENTF_LEFTDOWN = 0x0002;
    public const uint MOUSEEVENTF_LEFTUP   = 0x0004;
}
"@

$script:failures = @()
$script:checks = 0
$script:foundHwnd = [IntPtr]::Zero

function Assert-That {
    param([string] $Name, [bool] $Condition, [string] $Detail = '')

    $script:checks++
    if ($Condition) {
        "  PASS  $Name"
    }
    else {
        $script:failures += $Name
        "  FAIL  $Name"
        if ($Detail) { "        $Detail" }
    }
}

function Find-Descendant {
    param([IntPtr] $Parent, [string] $ClassName)

    $script:foundHwnd = [IntPtr]::Zero
    $callback = [MdppCopyProbe+EnumProc] {
        param($hwnd, $param)
        $builder = New-Object System.Text.StringBuilder 256
        [void][MdppCopyProbe]::GetClassName($hwnd, $builder, $builder.Capacity)
        if ($builder.ToString() -eq $ClassName) {
            $script:foundHwnd = $hwnd
            return $false
        }
        return $true
    }

    [void][MdppCopyProbe]::EnumChildWindows($Parent, $callback, [IntPtr]::Zero)
    return $script:foundHwnd
}

function Copy-FromPreviewPane {
    if (-not (Test-Path -LiteralPath $NotepadExe)) { throw "Notepad++ not found at $NotepadExe" }
    if (-not (Test-Path -LiteralPath $File)) { throw "Fixture not found: $File" }

    Start-Process -FilePath $NotepadExe -ArgumentList $File | Out-Null

    $main = [IntPtr]::Zero
    $deadline = (Get-Date).AddSeconds(30)
    while ((Get-Date) -lt $deadline) {
        $process = Get-Process -Name 'notepad++' -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($process -and $process.MainWindowHandle -ne [IntPtr]::Zero) { $main = $process.MainWindowHandle; break }
        Start-Sleep -Milliseconds 400
    }
    if ($main -eq [IntPtr]::Zero) { throw 'Notepad++ main window never appeared.' }

    $preview = [IntPtr]::Zero
    $deadline = (Get-Date).AddSeconds(30)
    while ((Get-Date) -lt $deadline) {
        $preview = Find-Descendant -Parent $main -ClassName $PreviewClass
        if ($preview -ne [IntPtr]::Zero -and [MdppCopyProbe]::IsWindowVisible($preview)) { break }
        Start-Sleep -Milliseconds 400
    }

    if ($preview -eq [IntPtr]::Zero -or -not [MdppCopyProbe]::IsWindowVisible($preview)) {
        [void][MdppCopyProbe]::SetForegroundWindow($main)
        Start-Sleep -Milliseconds 600
        (New-Object -ComObject WScript.Shell).SendKeys('^+m')
        Start-Sleep -Seconds 3
        $preview = Find-Descendant -Parent $main -ClassName $PreviewClass
    }

    if ($preview -eq [IntPtr]::Zero) {
        throw "Preview window class '$PreviewClass' was never found. Is the plugin INSTALLED, not just built?"
    }

    $rect = New-Object MdppCopyProbe+RECT
    [void][MdppCopyProbe]::GetWindowRect($preview, [ref] $rect)
    if ($rect.Right -le $rect.Left -or $rect.Bottom -le $rect.Top) { throw 'Preview pane has an empty rect.' }

    # Give WebView2 time to navigate and let Mermaid finish.
    Start-Sleep -Seconds 4

    [System.Windows.Forms.Clipboard]::SetText($Sentinel)

    # Top-right inside the pane is margin whitespace for this fixture, so the click cannot
    # land on a link and navigate away.
    [void][MdppCopyProbe]::SetForegroundWindow($main)
    Start-Sleep -Milliseconds 500
    [void][MdppCopyProbe]::SetCursorPos($rect.Right - 14, $rect.Top + 14)
    Start-Sleep -Milliseconds 250
    [MdppCopyProbe]::mouse_event([MdppCopyProbe]::MOUSEEVENTF_LEFTDOWN, 0, 0, 0, [IntPtr]::Zero)
    [MdppCopyProbe]::mouse_event([MdppCopyProbe]::MOUSEEVENTF_LEFTUP, 0, 0, 0, [IntPtr]::Zero)
    Start-Sleep -Milliseconds 600

    $shell = New-Object -ComObject WScript.Shell
    $shell.SendKeys('^a')
    Start-Sleep -Milliseconds 700
    $shell.SendKeys('^c')
    Start-Sleep -Milliseconds 1500
}

"Markdown++ preview copy check"
"  fixture: $File"
''

Copy-FromPreviewPane

$data = [System.Windows.Forms.Clipboard]::GetDataObject()
if (-not $data) { throw 'Clipboard unavailable.' }

$plain = ''
if ($data.GetDataPresent('UnicodeText')) { $plain = [string]$data.GetData('UnicodeText') }

$html = ''
if ($data.GetDataPresent('HTML Format')) {
    $raw = $data.GetData('HTML Format')
    if ($raw -is [System.IO.Stream]) {
        $raw = (New-Object System.IO.StreamReader($raw, [System.Text.Encoding]::UTF8)).ReadToEnd()
    }
    $html = [string]$raw
}

'Clipboard'
Assert-That 'the copy produced something' ($plain -ne $Sentinel) 'The sentinel survived, so nothing was copied at all.'
Assert-That 'an HTML flavour is present' ($html.Length -gt 0)
Assert-That 'the preview theme colour did NOT travel' (-not $html.Contains($DarkThemeForeground)) "Payload contains $DarkThemeForeground, so a paste target will render near-white text."
Assert-That 'emphasis survives as markup' ($html.Contains('<strong>') -and $html.Contains('<em>'))
Assert-That 'the page scripts were not copied' (-not $plain.Contains('MarkdownPlusPlusOptions'))
$plainLines = $plain -split "`r?`n"
$headingIsOwnBlock = $plainLines.Count -gt 2 -and
                     $plainLines[0].Trim() -eq 'Clipboard copy fixture' -and
                     $plainLines[1].Trim() -eq ''
Assert-That 'the first heading is its own block in the plain-text flavour' $headingIsOwnBlock "First two lines were '$($plainLines[0])' / '$($plainLines[1])'. A heading running straight into the next block is the 'clumped together' symptom."
Assert-That 'blocks are separated throughout the plain-text flavour' ((([regex]::Matches($plain, "`r?`n`r?`n")).Count) -ge 5)
Assert-That 'the Mermaid source is in the plain-text flavour' ($plain.Contains('graph TD'))

''
'Word paste'

$word = $null
$doc = $null
$wordAvailable = $true
try {
    $word = New-Object -ComObject Word.Application
}
catch {
    $wordAvailable = $false
}

if (-not $wordAvailable) {
    '  NOT RUN  Word is not available on this machine.'
    '            The clipboard assertions above ran; the paste assertions did NOT.'
    '            This run is DEGRADED, not green.'
}
else {
    try {
        $word.Visible = $false
        $word.DisplayAlerts = 0
        $doc = $word.Documents.Add()
        $doc.Content.Paste()

        function Get-PastedRange {
            param([string] $Text)
            $range = $doc.Content
            $range.Find.ClearFormatting()
            $range.Find.Forward = $true
            $range.Find.Wrap = 0
            $range.Find.Text = $Text
            if ($range.Find.Execute()) { return $range }
            return $null
        }

        $boldRange = Get-PastedRange -Text 'bold text'
        $italicRange = Get-PastedRange -Text 'italic text'

        Assert-That 'bold survived the paste' ($null -ne $boldRange -and $boldRange.Bold -eq -1)
        Assert-That 'italic survived the paste' ($null -ne $italicRange -and $italicRange.Italic -eq -1)
        Assert-That 'the markdown table became a real Word table' ($doc.Tables.Count -ge 1)

        if ($boldRange) {
            # wdColorAutomatic is -16777216. Anything else means the preview forced a colour,
            # and a light one on a white page is the invisible-text defect.
            #
            # TREAT THIS AS CORROBORATION, NOT AS THE GATE. Mutation-tested 2026-10-02 by
            # disabling the copy handler: this check still PASSED while the defect was live,
            # because for that selection Chromium hoisted the shared colour onto a wrapper span
            # and Word discarded the wrapper. Whether the defect reaches Word depends on what
            # was selected. The authoritative check is the payload one above, which fails every
            # time. A check the defect can walk past is worth keeping only if you know it can.
            Assert-That 'pasted text uses the target document colour' ($boldRange.Font.Color -eq -16777216) "Font colour came through as $($boldRange.Font.Color) (BGR) instead of automatic."
        }
    }
    finally {
        if ($doc) { $doc.Close(0) | Out-Null }
        if ($word) { $word.Quit() | Out-Null }
    }
}

if (-not $KeepNotepadOpen) {
    $running = Get-Process -Name 'notepad++' -ErrorAction SilentlyContinue
    if ($running) { $running.CloseMainWindow() | Out-Null }
}

''
if ($script:failures.Count -eq 0) {
    "All $($script:checks) checks passed."
    if (-not $wordAvailable) { 'NOTE: the run was DEGRADED, Word assertions did not execute.' }
    exit 0
}

"$($script:failures.Count) of $($script:checks) checks FAILED:"
$script:failures | ForEach-Object { '  - ' + $_ }
exit 1
