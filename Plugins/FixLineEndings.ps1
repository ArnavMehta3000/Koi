# PowerShell script to fix line endings (LF -> CRLF)

param(
    [Parameter(Mandatory=$true)]
    [string]$Root,

    [int]$TabSize = 4
)

$extensions = @(
    ".cpp",".c",".h",".hpp",".inl",".ixx",".ipp",
    ".cs",".txt",".cmake",".py",".json",".xml",
    ".yml",".yaml",".md", ".lua", ".toml"
)

$encoding = New-Object System.Text.UTF8Encoding($false)    # UTF-8 without BOM
# $encoding = New-Object System.Text.UTF8Encoding($true)   # UTF-8 with BOM

Get-ChildItem $Root -Recurse -File |
Where-Object { $extensions -contains $_.Extension.ToLower() } |
ForEach-Object {

    $text = [System.IO.File]::ReadAllText($_.FullName)

    # Normalize to LF
    $text = $text.Replace("`r`n","`n").Replace("`r","`n")

    # Convert leading spaces to tabs
    $lines = $text.Split("`n")

    for ($i = 0; $i -lt $lines.Length; $i++)
    {
        if ($lines[$i] -match '^( +)')
        {
            $count = $Matches[1].Length
            $tabs = [math]::Floor($count / $TabSize)
            $spaces = $count % $TabSize

            $lines[$i] =
                ("`t" * $tabs) +
                (" " * $spaces) +
                $lines[$i].Substring($count)
        }
    }

    # Force CRLF
    $text = [string]::Join("`r`n", $lines)

    [System.IO.File]::WriteAllText($_.FullName, $text, $encoding)

    Write-Host "Updated $($_.FullName)"
}
