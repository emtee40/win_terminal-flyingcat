
function get_ct($file, $first, $last)
{
    if ($file -is [string]) {
        Get-Content $file | % {$i=1} { if ($i -ge $first -and $i -le $last) { $_ }; $i++ }
    }
    else {
        $first..$last | % { $file[$_ - 1] }
    }
}


$file = Join-Path $PSScriptRoot '../../parser/OutputStateMachineEngine.hpp'
$arr = get_ct $file 106 180 # CsiActionCodes
$dict = @{}
$arr | ?{$_ -match '(\w+) = VTID\("([^)]+)"\)'} | % {
    $ct = $Matches[2]
    if ($ct -like "*'*") {
        $ct = '"' + $ct + '"'
    }
    else {
        $ct = "'" + $ct + "'"
    }
    $ct = $ct -replace '\\"','"'
    $dict[$Matches[1]] = $ct
}
if ($dict.Count -ne $arr.Length) { throw }


$cpp = Get-Content (Join-Path $PSScriptRoot '../../parser/OutputStateMachineEngine.cpp')

$codes = @()
$lines = @()
$arr = get_ct $cpp 444 689 | %{
    if ($_ -match 'case CsiActionCodes::(\w+):') {
        $codes += @($Matches[1])
    }
    elseif ($_.Trim() -eq 'break;') {
        [PSCustomObject]@{ codes = $codes; lines = $lines }
        $codes = @()
        $lines = @()
    }
    else {
        $lines += @(($_ -replace '^    ','') -replace '_dispatch->','_dispatch.')
    }
}

$arr | %{
    "[[msvc::forceinline]] bool Csi_$($_.codes -join '_')(const VTParameters parameters)"
    '{'
    '    bool success;'
    $_.lines
    '    return success;'
    '}'
    ''
}
''
'# # # # # # # # # # # # # # # # # # # #'
''
$arr | %{
    ($_.codes -join '_') + ' = ' + (($_.codes | %{$dict[$_]}) -join ', ')
}


''
''
'* * * * * * * * * * * * * * * * * * * *'
''
''

$file = Join-Path $PSScriptRoot '../../parser/ascii.hpp'
$arr = get_ct $file 10 43 # AsciiChars
$dict = @{}
$arr | ?{$_ -match '(\w+) = (\w+)'} | %{$dict[$Matches[1]] = $Matches[2]}
if ($dict.Count -ne $arr.Length) { throw }


$codes = @()
$lines = @()
$arr = get_ct $cpp 52 99 | %{
    if ($_ -match 'case AsciiChars::(\w+):') {
        $codes += @($Matches[1])
    }
    elseif ($_.Trim() -eq 'break;') {
        [PSCustomObject]@{ codes = $codes; lines = $lines }
        $codes = @()
        $lines = @()
    }
    else {
        $lines += @($_ -replace '^    ','')
    }
}

$arr | %{
    "[[msvc::forceinline]] bool ActionExecute_$($_.codes -join '_')()"
    '{'
    $_.lines
    '    return true;'
    '}'
    ''
}
''
'# # # # # # # # # # # # # # # # # # # #'
''
$arr | %{
    ($_.codes -join '_') + ' = ' + (($_.codes | %{$dict[$_]}) -join ', ')
}
