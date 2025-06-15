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
  // For secure token storage
  keytar: {
    setPassword: (service, account, password) => 
      ipcRenderer.invoke('keytar:setPassword', service, account, password),
    getPassword: (service, account) => 
      ipcRenderer.invoke('keytar:getPassword', service, account),
    deletePassword: (service, account) => 
      ipcRenderer.invoke('keytar:deletePassword', service, account),
  },
  // We can add more APIs here as needed
});
