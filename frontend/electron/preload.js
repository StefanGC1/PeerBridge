const { contextBridge, ipcRenderer } = require('electron');

// Expose protected methods that allow the renderer process to use
// the ipcRenderer without exposing the entire object
contextBridge.exposeInMainWorld('electron', {
  // From renderer to main process
  send: (channel, data) => {
    // Whitelist channels
    const validChannels = [
      'p2p:connect', 
      'p2p:disconnect', 
      'p2p:get-peers', 
      'p2p:send-message'
    ];
    if (validChannels.includes(channel)) {
      ipcRenderer.send(channel, data);
    }
  },
  
  // From main process to renderer
  receive: (channel, func) => {
    const validChannels = [
      'p2p:status-update', 
      'p2p:peer-update', 
      'p2p:message-received',
      'p2p:error'
    ];
    if (validChannels.includes(channel)) {
      // Deliberately strip event as it includes `sender` 
      ipcRenderer.on(channel, (event, ...args) => func(...args));
    }
  },
  
  // Invoke methods (two-way communication)
  invoke: async (channel, data) => {
    const validChannels = [
      'p2p:get-connection-status', 
      'p2p:get-peer-list'
    ];
    if (validChannels.includes(channel)) {
      return await ipcRenderer.invoke(channel, data);
    }
    
    return null;
  }
});