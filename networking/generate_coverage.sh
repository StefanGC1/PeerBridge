#!/bin/bash

# BE CAREFUL WHEN RUNNING THIS SCRIPT

set -e

echo "=== Code Coverage Report Generation ==="

# Configuration
BUILD_DIR="build"
COVERAGE_DIR="coverage"
SOURCE_DIR="src"

# Clean previous coverage data
echo "Cleaning previous coverage data..."
find . -name "*.gcda" -delete
find . -name "*.gcno" -delete
rm -rf ${COVERAGE_DIR}

# Create build directory
mkdir -p ${BUILD_DIR}
cd ${BUILD_DIR}

# Configure with coverage enabled
echo "Configuring CMake with coverage enabled..."
cmake .. -DENABLE_COVERAGE=ON -DBUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug -G "MinGW Makefiles"

# Build the project
echo "Building project..."
mingw32-make -j$(nproc)

# Run tests to generate coverage data
echo "Running tests to generate coverage data..."
./tests/PeerBridgeNet_tests.exe

# Go back to root directory
cd ..

# Create coverage directory
mkdir -p ${COVERAGE_DIR}

# Generate initial coverage info
echo "Generating coverage info..."
lcov --capture --directory ${BUILD_DIR} --output-file ${COVERAGE_DIR}/coverage.info --gcov-tool gcov

# Remove external libraries and system files from coverage
echo "Filtering coverage data..."
lcov --remove ${COVERAGE_DIR}/coverage.info \
    '/usr/*' \
    '/mingw64/*' \
    '/c/msys64/*' \
    '*/build/*' \
    '*/tests/*' \
    '*/proto/*' \
    '*/wintun/*' \
    --output-file ${COVERAGE_DIR}/coverage_filtered.info

# Generate HTML report
echo "Generating HTML coverage report..."
genhtml ${COVERAGE_DIR}/coverage_filtered.info --output-directory ${COVERAGE_DIR}/html

# Generate console summary
echo "Generating coverage summary..."
lcov --summary ${COVERAGE_DIR}/coverage_filtered.info

echo ""
echo "=== Coverage Report Generated ==="
echo "HTML Report: ${COVERAGE_DIR}/html/index.html"
echo "Raw Data: ${COVERAGE_DIR}/coverage_filtered.info"
echo ""
echo "Open the HTML report in your browser to view detailed coverage information." 