# Check if output filename is provided
param (
    [Parameter(Mandatory=$true,
               Position=0,
               HelpMessage="Output firmware filename")]
    [string]$OutputFile
)

# Display script information
Write-Host "Merging ESP32-S3 firmware..."
Write-Host "Output file: $OutputFile"

# Check if build directory exists
if (-not (Test-Path "build")) {
    Write-Host "Error: 'build' directory not found" -ForegroundColor Red
    exit 1
}

# Check if flash_args exists
if (-not (Test-Path "build/flash_args")) {
    Write-Host "Error: 'flash_args' file not found in build directory" -ForegroundColor Red
    exit 1
}

# Change to build directory
Set-Location -Path "build"

try {
    # Check if esptool.py is available
    $esptoolVersion = & esptool.py version 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Error: esptool.py not found or not in PATH" -ForegroundColor Red
        exit 1
    }
    Write-Host "Using $esptoolVersion"

    # Execute esptool.py merge command
    Write-Host "Merging firmware files..." -ForegroundColor Yellow
    esptool.py --chip esp32s3 merge_bin --output $OutputFile "@flash_args"
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host "Firmware merge successful: $OutputFile" -ForegroundColor Green
        
        # Show file size
        $fileSize = (Get-Item $OutputFile).Length
        $fileSizeMB = [math]::Round($fileSize / 1MB, 2)
        Write-Host "Output file size: $fileSizeMB MB" -ForegroundColor Green
    } else {
        Write-Host "Error: Firmware merge failed" -ForegroundColor Red
        exit 1
    }
} catch {
    Write-Host "Error: $_" -ForegroundColor Red
    exit 1
} finally {
    # Return to original directory
    Set-Location -Path ".."
}