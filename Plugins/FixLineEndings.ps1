# PowerShell script to normalize source file encoding (UTF-8 w/ BOM) and line endings (CRLF).
#
param(
	[Parameter(Mandatory=$true)]
	[string]$Root,

	# Write UTF-8 without a BOM instead. Note that MSVC then assumes the active
	# code page for the source file unless you also pass /utf-8 to cl.exe.
	[switch]$NoBom
)

$extensions = @(
	".cpp", ".c", ".h", ".hpp", ".inl", ".ixx", ".json", ".xml",
	".md", ".lua", ".toml", ".hlsl", ".hlsli"
)

# Encoding used for writing.
$encoding = New-Object System.Text.UTF8Encoding((-not $NoBom))

# Encoding used for reading: throws on invalid UTF-8 rather than silently
# substituting U+FFFD, so we never destroy bytes from a legacy-encoded file
$strictUtf8 = New-Object System.Text.UTF8Encoding($false, $true)

$updated = 0
$skipped = 0

Get-ChildItem $Root -Recurse -File |
Where-Object { $extensions -contains $_.Extension.ToLower() } |
ForEach-Object {

	$path  = $_.FullName
	$bytes = [System.IO.File]::ReadAllBytes($path)

	try
	{
		$text = $strictUtf8.GetString($bytes)
	}
	catch [System.Text.DecoderFallbackException]
	{
		Write-Warning "Not valid UTF-8, skipping (fix by hand): $path"
		$skipped++
		return
	}

	# Strip any existing BOM; the writer re-adds it if requested
	if ($text.Length -gt 0 -and $text[0] -eq [char]0xFEFF)
	{
		$text = $text.Substring(1)
	}

	# Normalize to LF, then force CRLF
	$text = $text.Replace("`r`n", "`n").Replace("`r", "`n")
	$text = $text.Replace("`n", "`r`n")

	$newBytes = [byte[]]($encoding.GetPreamble() + $encoding.GetBytes($text))

	# Only write when the bytes actually change, so untouched files keep their
	# timestamps and don't trigger a full rebuild
	if ([Convert]::ToBase64String($bytes) -ne [Convert]::ToBase64String($newBytes))
	{
		[System.IO.File]::WriteAllBytes($path, $newBytes)
		Write-Host "Updated $path"
		$updated++
	}
}

Write-Host ""
Write-Host "Done. $updated file(s) updated, $skipped skipped."
