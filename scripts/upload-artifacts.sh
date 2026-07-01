#!/bin/bash
set -euo pipefail

WINDOWS_BUILD_DIRECTORY="build-windows"
WINDOWS_CLI_BINARY_PATH="${WINDOWS_BUILD_DIRECTORY}/catupdate.exe"
WINDOWS_GUI_BINARY_PATH="${WINDOWS_BUILD_DIRECTORY}/catupdate-gui.exe"

CURRENT_DATE_TIMESTAMP=$(date +'%Y-%m-%d_%H-%M-%S')
WINDOWS_ZIP_ARCHIVE_PATH="${WINDOWS_BUILD_DIRECTORY}/catupdate-windows-${CURRENT_DATE_TIMESTAMP}.zip"

# Optional upload; exit gracefully if URL is not configured.
if [ -z "${ARTIFACTS_UPLOAD_URL:-}" ]; then
    echo "Warning: ARTIFACTS_UPLOAD_URL environment variable is not defined or is empty. Skipping optional upload."
    exit 0
fi

if [[ ! "$ARTIFACTS_UPLOAD_URL" =~ ^https:// ]]; then
    echo "Error: ARTIFACTS_UPLOAD_URL must use the secure 'https://' protocol." >&2
    exit 1
fi

if [ ! -f "$WINDOWS_CLI_BINARY_PATH" ]; then
    echo "Error: Windows CLI binary not found: $WINDOWS_CLI_BINARY_PATH" >&2
    exit 1
fi

if [ ! -f "$WINDOWS_GUI_BINARY_PATH" ]; then
    echo "Error: Windows GUI binary not found: $WINDOWS_GUI_BINARY_PATH" >&2
    exit 1
fi

if ! command -v zip &> /dev/null; then
    echo "Error: The 'zip' utility is required but not installed." >&2
    exit 1
fi

echo "Compressing Windows executable binaries..."
zip -j "$WINDOWS_ZIP_ARCHIVE_PATH" "$WINDOWS_CLI_BINARY_PATH" "$WINDOWS_GUI_BINARY_PATH"
echo "ZIP archive successfully created: $WINDOWS_ZIP_ARCHIVE_PATH"

echo "Uploading ZIP archive to S3-compatible destination..."
curl \
    --proto "=https" \
    --tlsv1.2 \
    --fail \
    --show-error \
    --silent \
    --upload-file "$WINDOWS_ZIP_ARCHIVE_PATH" \
    "$ARTIFACTS_UPLOAD_URL"

echo "Upload completed successfully."
