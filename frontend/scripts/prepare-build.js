import { existsSync, mkdirSync } from 'fs';
import { join } from 'path';
import { execSync } from 'child_process';
import { fileURLToPath } from 'url';
import path from 'path';

// Define paths
const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
console.log(__dirname)

const CPP_DIR = join(__dirname, '../cpp');
const PROTO_DIR = join(__dirname, '../proto');
const DIST_DIR = join(__dirname, '../dist');

// Ensure the dist directory exists
if (!existsSync(DIST_DIR)) {
  mkdirSync(DIST_DIR, { recursive: true });
}

// Create cpp directory in dist if it doesn't exist
const DIST_CPP_DIR = join(DIST_DIR, 'cpp');
if (!existsSync(DIST_CPP_DIR)) {
  mkdirSync(DIST_CPP_DIR, { recursive: true });
}

// Create proto directory in dist if it doesn't exist
const DIST_PROTO_DIR = join(DIST_DIR, 'proto');
if (!existsSync(DIST_PROTO_DIR)) {
  mkdirSync(DIST_PROTO_DIR, { recursive: true });
}

// Copy cpp directory contents to dist
console.log('Copying cpp directory to dist...');
execSync(`xcopy /S /Y /I "${CPP_DIR}" "${DIST_CPP_DIR}"`);

// Copy proto directory contents to dist
console.log('Copying proto directory to dist...');
execSync(`xcopy /S /Y /I "${PROTO_DIR}" "${DIST_PROTO_DIR}"`);

console.log('Build preparation complete.'); 