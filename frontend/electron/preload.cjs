const { contextBridge, ipcRenderer } = require('electron');

// Expose protected methods that allow the renderer process to use
// the ipcRenderer without exposing the entire object
contextBridge.exposeInMainWorld('electron', {
  // For network module communication
  grpc: {
    getStunInfo: () => ipcRenderer.invoke('grpc:getStunInfo'),
    startConnection: (peerInfo, selfIndex, shouldFail) => 
      ipcRenderer.invoke('grpc:startConnection', peerInfo, selfIndex, shouldFail),
    stopConnection: () => ipcRenderer.invoke('grpc:stopConnection'),
    cleanup: () => ipcRenderer.invoke('grpc:cleanup'),
  },
  // We can add more APIs here as needed
});
