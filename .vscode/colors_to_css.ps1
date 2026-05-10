param([string]$File)
$content = Get-Content $File -Raw
$content = $content -replace '0x([0-9a-fA-F]{6})\b', '#$1'
Set-Content $File $content -NoNewline
