$c = @'
call feedkeys("$", "x")
let cc = 1
while 1
    call feedkeys("\<C-F>", "x")
    redraw
    if (cc % 100) == 0
        if line("$") == line(".")
            break
        endif
    endif
    let cc = cc + 1
endwhile
quit
'@
$c = $c -replace "`r",'' # surprise

$vim = Get-Command vim -ErrorAction Ignore
if (-not $vim) {
    $vim = Get-Command nvim -ErrorAction Stop
}

Invoke-WebRequest 'https://cdn.jsdelivr.net/npm/typescript@2.2.2/lib/tsc.js' -OutFile VT_EN_P.js
&$vim -c $c -R VT_EN_P.js > VT_EN_V
Copy-Item VT_EN_P.js VT_EN_P -Force
Remove-Item VT_EN_P.js

$file = New-Item VT_CN_P.html -Force
@(
    'https://zh.wikipedia.org/zh-cn/%E7%A7%A6%E6%9C%9D'
    'https://zh.wikipedia.org/zh-cn/%E6%B1%89%E6%9C%9D'
    'https://zh.wikipedia.org/zh-cn/%E9%9A%8B%E6%9C%9D'
    'https://zh.wikipedia.org/zh-cn/%E5%94%90%E6%9C%9D'
) | % { Invoke-RestMethod $_ } | Out-File $file -Append
&$vim -c $c -R VT_CN_P.html > VT_CN_V
Copy-Item VT_CN_P.html VT_CN_P -Force
Remove-Item VT_CN_P.html

