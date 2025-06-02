const path = require('path');
const { spawn } = require('child_process');
const grpc = require('@grpc/grpc-js');
const protoLoader = require('@grpc/proto-loader');
const sudoPrompt = require('sudo-prompt');
const isDev = require('electron-is-dev');

let networkProcess = null;
let grpcClient = null;
const options = { name: 'PeerBridge' };

// Load the protobuf
const PROTO_PATH = path.join(__dirname, '../proto/peerbridge.proto');
const packageDefinition = protoLoader.loadSync(PROTO_PATH, {
  keepCase: true,
  longs: String,
  enums: String,
  defaults: true,
  oneofs: true
});
const protoDescriptor = grpc.loadPackageDefinition(packageDefinition);
const peerbridge = protoDescriptor.peerbridge;

// Function to start the C++ networking module with elevated privileges
function startNetworkingModule() {
  return new Promise((resolve, reject) => {
    const executablePath = isDev
      ? path.join(__dirname, '../cpp/bin/PeerBridgeNet.exe')
      : path.join(process.resourcesPath, 'cpp/bin/PeerBridgeNet.exe');

    // Use sudo-prompt for elevated privileges
    sudoPrompt.exec(`"${executablePath}"`, options, (error, stdout, stderr) => {
      if (error) {
        console.error('Error starting network module:', error);
        reject(error);
        return;
      }
      
      console.log('Network module started successfully');
      resolve();
    });
  });
}

// Connect to the gRPC server
function connectGrpcClient() {
  if (grpcClient) return grpcClient;
  
  // Create client using proto
  grpcClient = new peerbridge.PeerBridgeService(
    'localhost:50051', 
    grpc.credentials.createInsecure()
  );
  
  return grpcClient;
}

// Get STUN info (public IP and port)
function getStunInfo() {
  return new Promise((resolve, reject) => {
    const client = connectGrpcClient();
    
    client.getStunInfo({}, (error, response) => {
      if (error) {
        console.error('Error getting STUN info:', error);
        reject(error);
        return;
      }
      
      resolve({
        publicIp: response.public_ip,
        publicPort: response.public_port,
        errorMessage: response.error_message
      });
    });
  });
}

// Initialize the networking module and gRPC connection
async function initializeNetworking() {
  try {
    await startNetworkingModule();
    connectGrpcClient();
    return true;
  } catch (error) {
    console.error('Failed to initialize networking:', error);
    return false;
  }
}

// Cleanup function
function cleanup() {
  if (networkProcess) {
    networkProcess.kill();
    networkProcess = null;
  }
  grpcClient = null;
}

module.exports = {
  initializeNetworking,
  getStunInfo,
  cleanup
};
