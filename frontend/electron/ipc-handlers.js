const { spawn } = require('child_process');
const path = require('path');
const isDev = require('electron-is-dev');

// This will hold the reference to the C++ process
let cppBackendProcess = null;

/**
 * Starts the C++ backend process
 */
function startCppBackend() {
  // Path to the C++ executable
  // In development, it might be in a different location than in production
  const cppExecutablePath = isDev
    ? path.join(__dirname, '../../backend/bin/p2p-backend')
    : path.join(process.resourcesPath, 'backend/bin/p2p-backend');
  
  // Additional arguments to pass to the C++ process
  const args = ['--log-level', 'info'];
  
  try {
    // Start the C++ process
    cppBackendProcess = spawn(cppExecutablePath, args);
    
    // Handle standard output from the C++ process
    cppBackendProcess.stdout.on('data', (data) => {
      try {
        // Parse the JSON data from the C++ process
        const message = JSON.parse(data.toString());
        
        // Handle different message types
        if (message.type === 'status') {
          // Forward status updates to renderer process
          global.mainWindow.webContents.send('p2p:status-update', message.data);
        } else if (message.type === 'peer') {
          // Forward peer updates to renderer process
          global.mainWindow.webContents.send('p2p:peer-update', message.data);
        } else if (message.type === 'message') {
          // Forward received messages to renderer process
          global.mainWindow.webContents.send('p2p:message-received', message.data);
        }
      } catch (error) {
        console.error('Error parsing message from C++ process:', error);
        console.log('Raw message:', data.toString());
      }
    });
    
    // Handle errors from the C++ process
    cppBackendProcess.stderr.on('data', (data) => {
      console.error(`C++ backend error: ${data}`);
      // Forward error to renderer process
      global.mainWindow.webContents.send('p2p:error', {
        message: `Backend error: ${data}`
      });
    });
    
    // Handle C++ process exit
    cppBackendProcess.on('close', (code) => {
      console.log(`C++ backend process exited with code ${code}`);
      cppBackendProcess = null;
    });
    
    return true;
  } catch (error) {
    console.error('Failed to start C++ backend:', error);
    return false;
  }
}

/**
 * Stops the C++ backend process
 */
function stopCppBackend() {
  if (cppBackendProcess) {
    cppBackendProcess.kill();
    cppBackendProcess = null;
  }
}

/**
 * Sends a command to the C++ backend
 * @param {string} command - The command to send
 * @param {Object} data - The data to send with the command
 * @returns {Promise<boolean>} - Whether the command was sent successfully
 */
function sendToCppBackend(command, data) {
  return new Promise((resolve, reject) => {
    if (!cppBackendProcess) {
      const started = startCppBackend();
      if (!started) {
        reject(new Error('Failed to start C++ backend'));
        return;
      }
    }
    
    try {
      const message = JSON.stringify({
        command,
        data,
        timestamp: Date.now()
      });
      
      cppBackendProcess.stdin.write(message + '\n');
      resolve(true);
    } catch (error) {
      console.error('Error sending message to C++ backend:', error);
      reject(error);
    }
  });
}

/**
 * Sets up IPC handlers for communication between renderer and main process
 * @param {Object} ipcMain - The Electron ipcMain object
 */
function setupIpcHandlers(ipcMain) {
  // Handle connect request from renderer
  ipcMain.on('p2p:connect', async (event, data) => {
    try {
      await sendToCppBackend('connect', data);
    } catch (error) {
      event.sender.send('p2p:error', {
        message: 'Failed to connect to P2P network',
        details: error.message
      });
    }
  });
  
  // Handle disconnect request from renderer
  ipcMain.on('p2p:disconnect', async (event) => {
    try {
      await sendToCppBackend('disconnect', {});
    } catch (error) {
      event.sender.send('p2p:error', {
        message: 'Failed to disconnect from P2P network',
        details: error.message
      });
    }
  });
  
  // Handle get peers request from renderer
  ipcMain.on('p2p:get-peers', async (event) => {
    try {
      await sendToCppBackend('get-peers', {});
    } catch (error) {
      event.sender.send('p2p:error', {
        message: 'Failed to get peer list',
        details: error.message
      });
    }
  });
  
  // Handle send message request from renderer
  ipcMain.on('p2p:send-message', async (event, data) => {
    try {
      await sendToCppBackend('send-message', data);
    } catch (error) {
      event.sender.send('p2p:error', {
        message: 'Failed to send message',
        details: error.message
      });
    }
  });
  
  // Handle connection status request (invoke)
  ipcMain.handle('p2p:get-connection-status', async () => {
    try {
      // In a real implementation, this would query the C++ backend
      // For now, we'll return a placeholder status
      return { status: 'disconnected' };
    } catch (error) {
      console.error('Error getting connection status:', error);
      throw error;
    }
  });
  
  // Handle peer list request (invoke)
  ipcMain.handle('p2p:get-peer-list', async () => {
    try {
      // In a real implementation, this would query the C++ backend
      // For now, we'll return a placeholder peer list
      return { peers: [] };
    } catch (error) {
      console.error('Error getting peer list:', error);
      throw error;
    }
  });
}

// Clean up when app exits
process.on('exit', () => {
  stopCppBackend();
});

// Handle uncaught exceptions
process.on('uncaughtException', (error) => {
  console.error('Uncaught exception:', error);
  stopCppBackend();
});

module.exports = {
  setupIpcHandlers,
  startCppBackend,
  stopCppBackend
};