
function get_ct($file, $first, $last)
{
    if ($file -is [string]) {
        Get-Content $file | % {$i=1} { if ($i -ge $first -and $i -le $last) { $_ }; $i++ }
    }
    else {
        $first..$last | % { $file[$_ - 1] }
    }
}

$file = Join-Path $PSScriptRoot '../../adapter/ITermDispatch.hpp'
$content = get_ct $file 32 174
$content = ($content -join "`n") -replace ',\n\s*',', '

$actions = @()

$content -split '\n' | ?{ $_ -match 'virtual (\w+) (\w+)\(' } | %{
    $retType = $Matches[1]
    $params = @('Action::'+$Matches[2])
    $actions += @($Matches[2])
    $params += @($_ -replace '//.*$','' | select-string '(\w+)[,)]' -AllMatches | % Matches | %{ $_.Groups[1].value })
    '[[msvc::noinline]] ' + (($_ -replace '= 0;','override') -replace '^\s*virtual ','')
    '{'
    "    _data.push($($params -join ', '));"
    if ($retType -ne 'void') {
        "    return $($retType -eq 'bool' ? 'true' : 'nullptr');"
    }
    '}'
    ''
}

'enum class Action : uint8_t'
'{'
$actions | %{ "    $_," }
'};'
