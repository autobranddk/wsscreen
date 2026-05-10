param([string]$File)
$content = Get-Content $File -Raw
$content = $content -replace '#([0-9a-fA-F]{6})\b', '0x$1'
Set-Content $File $content -NoNewline
