import { app, BrowserWindow, ipcMain } from 'electron';
import { join } from 'path';
import isDev from 'electron-is-dev';
import { spawn } from 'child_process';
import { initializeNetworking, getStunInfo, cleanup } from './grpc.cjs';

import path from 'path';

// Handle creating/removing shortcuts on Windows when installing/uninstalling
// if (require('electron-squirrel-startup')) {
//   app.quit();
// }

let __basePackagePath;
if (!isDev) {
  // Very weird electron packaging shenanigans
  console.log("Setting base app.asar path");
  __basePackagePath = app.getAppPath();
  console.log(__basePackagePath)
}

let mainWindow;

function createWindow() {
  // Create the browser window
  const preloadPath = join(
    app.getAppPath(), './electron/preload.cjs'); 
  mainWindow = new BrowserWindow({
    width: 1000,
    height: 900,
    webPreferences: {
      nodeIntegration: false,
      contextIsolation: true,
      enableRemoteModule: false,
      preload: preloadPath
    }
  });

  // Load app
  const startUrl = isDev 
    ? 'http://localhost:3000' 
    : `file://${join(__basePackagePath, 'dist/index.html')}`;
  
  mainWindow.loadURL(startUrl);

  // Open DevTools in development mode
  if (isDev) {
    mainWindow.webContents.openDevTools();
  }

  mainWindow.on('closed', () => {
    mainWindow = null;
  });
}

app.whenReady().then(async () => {
  createWindow();
  
  // Initialize networking and gRPC
  try {
    await initializeNetworking();
    console.log('Networking module initialized successfully');
  } catch (error) {
    console.error('Failed to initialize networking module:', error);
  }
  
  app.on('activate', () => {
    if (BrowserWindow.getAllWindows().length === 0) {
      createWindow();
    }
  });
});

// Handle IPC events
ipcMain.handle('grpc:getStunInfo', async () => {
  try {
    const stunInfo = await getStunInfo();
    return stunInfo;
  } catch (error) {
    console.error('Error in getStunInfo:', error);
    return { error: error.message || 'Failed to get STUN info' };
  }
});

ipcMain.handle('grpc:cleanup', async () => {
  try {
    await cleanup();
  } catch (error) {
    console.error('Error in stopProcess:', error);
    return { error: error.message || 'Failed to stop process' };
  }
});

app.on('window-all-closed', () => {
  if (process.platform !== 'darwin') {
    // Clean up the networking module
    cleanup();
    app.quit();
  }
});

app.on('before-quit', () => {
  // Ensure cleanup happens before quitting
  cleanup();
});