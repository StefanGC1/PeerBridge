@echo off

REM Define paths
set CPP_BUILD_DIR=..\networking\build\release
set CPP_PROTO_DIR=..\networking\proto
set DEST_DIR=cpp
set PROTO_DEST_DIR=proto

REM Create destination directory if it doesn't exist
if not exist %DEST_DIR% (
    mkdir %DEST_DIR%
    echo Created directory: %DEST_DIR%
) else (
    echo Directory already exists: %DEST_DIR%
)
if not exist %PROTO_DEST_DIR% (
    mkdir %PROTO_DEST_DIR%
    echo Created directory: %PROTO_DEST_DIR%
) else (
    echo Directory already exists: %PROTO_DEST_DIR%
)

REM Copy C++ build artifacts
xcopy /S /Y /I %CPP_BUILD_DIR% %DEST_DIR%\bin

REM TODO: Remove copying of the proto generated files
REM As they are not necessary
xcopy /S /Y /I %CPP_PROTO_DIR% %PROTO_DEST_DIR%

echo Copy complete. 