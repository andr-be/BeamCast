# PowerShell script to download and extract SDL2_ttf for Windows

$sdl2_ttf_url = "https://github.com/libsdl-org/SDL_ttf/releases/download/release-2.20.2/SDL2_ttf-2.20.2-win32-x64.zip"
$zip_file = "SDL2_ttf.zip"
$lib_dir = "lib"

Write-Host "Downloading SDL2_ttf..."
Invoke-WebRequest -Uri $sdl2_ttf_url -OutFile $zip_file

Write-Host "Extracting SDL2_ttf..."
Expand-Archive -Path $zip_file -DestinationPath $lib_dir -Force

# Rename extracted folder to SDL2_ttf
$extracted_folder = Get-ChildItem -Path $lib_dir -Filter "SDL2_ttf-*" | Select-Object -First 1
if ($extracted_folder) {
    Rename-Item -Path $extracted_folder.FullName -NewName "SDL2_ttf" -Force
}

Write-Host "Cleaning up..."
Remove-Item $zip_file

Write-Host "SDL2_ttf setup complete!"
