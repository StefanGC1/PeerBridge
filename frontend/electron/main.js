const { app, BrowserWindow, ipcMain } = require('electron');
const path = require('path');
const isDev = require('electron-is-dev');
const { spawn } = require('child_process');
const grpcModule = require('./grpc');

// Handle creating/removing shortcuts on Windows when installing/uninstalling
// if (require('electron-squirrel-startup')) {
//   app.quit();
// }

let mainWindow;

function createWindow() {
  // Create the browser window
  mainWindow = new BrowserWindow({
    width: 1000,
    height: 900,
    webPreferences: {
      nodeIntegration: false,
      contextIsolation: true,
      enableRemoteModule: false,
      preload: path.join(__dirname, 'preload.js')
    }
  });

  // Load app
  const startUrl = isDev 
    ? 'http://localhost:3000' 
    : `file://${path.join(__dirname, '../dist/index.html')}`;
  
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
    await grpcModule.initializeNetworking();
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
    const stunInfo = await grpcModule.getStunInfo();
    return stunInfo;
  } catch (error) {
    console.error('Error in getStunInfo:', error);
    return { error: error.message || 'Failed to get STUN info' };
  }
});

ipcMain.handle('grpc:cleanup', async () => {
  try {
    await grpcModule.cleanup();
  } catch (error) {
    console.error('Error in stopProcess:', error);
    return { error: error.message || 'Failed to stop process' };
  }
});

app.on('window-all-closed', () => {
  if (process.platform !== 'darwin') {
    // Clean up the networking module
    grpcModule.cleanup();
    app.quit();
  }
});

app.on('before-quit', () => {
  // Ensure cleanup happens before quitting
  grpcModule.cleanup();
});