# PowerShell script to download SDL2 for Windows
Write-Host "Downloading SDL2..." -ForegroundColor Green

$sdlVersion = "2.30.9"
$sdlUrl = "https://github.com/libsdl-org/SDL/releases/download/release-$sdlVersion/SDL2-devel-$sdlVersion-VC.zip"
$tempZip = "$env:TEMP\SDL2.zip"
$extractPath = "lib"

try {
    # Create lib directory if it doesn't exist
    New-Item -ItemType Directory -Force -Path $extractPath | Out-Null

    # Download SDL2
    Write-Host "Downloading from $sdlUrl..."
    Invoke-WebRequest -Uri $sdlUrl -OutFile $tempZip

    # Extract
    Write-Host "Extracting SDL2..."
    Expand-Archive -Path $tempZip -DestinationPath $extractPath -Force

    # Rename the extracted folder to just "SDL2"
    $extractedFolder = Get-ChildItem -Path $extractPath -Filter "SDL2-*" | Select-Object -First 1
    if ($extractedFolder) {
        Move-Item -Path $extractedFolder.FullName -Destination "$extractPath\SDL2" -Force
    }

    # Cleanup
    Remove-Item $tempZip -Force

    Write-Host "SDL2 setup complete!" -ForegroundColor Green
    Write-Host "You can now build the project with CMake."
}
catch {
    Write-Host "Error: $_" -ForegroundColor Red
    Write-Host ""
    Write-Host "Manual setup instructions:" -ForegroundColor Yellow
    Write-Host "1. Download SDL2 from: https://github.com/libsdl-org/SDL/releases/latest"
    Write-Host "2. Look for SDL2-devel-x.x.x-VC.zip"
    Write-Host "3. Extract to lib/SDL2/ in this directory"
}
