const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('electron', {

  grpc: {
    getStunInfo: () => ipcRenderer.invoke('grpc:getStunInfo'),
    startConnection: (peerInfo, selfIndex, shouldFail) => 
      ipcRenderer.invoke('grpc:startConnection', peerInfo, selfIndex, shouldFail),
    stopConnection: () => ipcRenderer.invoke('grpc:stopConnection'),
    cleanup: () => ipcRenderer.invoke('grpc:cleanup'),
  },

  // For storing refresh token
  keytar: {
    setPassword: (service, account, password) => 
      ipcRenderer.invoke('keytar:setPassword', service, account, password),
    getPassword: (service, account) => 
      ipcRenderer.invoke('keytar:getPassword', service, account),
    deletePassword: (service, account) => 
      ipcRenderer.invoke('keytar:deletePassword', service, account),
  },
});
