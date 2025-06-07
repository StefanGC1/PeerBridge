const path = require('path');
const { spawn } = require('child_process');
const grpc = require('@grpc/grpc-js');
const protoLoader = require('@grpc/proto-loader');
const isDev = require('electron-is-dev');
const { app } = require('electron')

let networkProcess = null;
let grpcClient = null;
const options = { name: 'PeerBridge' };

let __basePackagePath;
if (app.isPackaged) {
  // Very weird electron packaging shenanigans
  console.log("Setting base app.asar path");
  console.log(__dirname);
  __basePackagePath = path.join(__dirname, '..')
  console.log("From grpc.cjs " + __basePackagePath)
}

// Load the protobuf
const PROTO_PATH = isDev
  ? path.join(__dirname, '../proto/peerbridge.proto')
  : path.join(__basePackagePath, 'proto/peerbridge.proto');

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
// Refactorize: move to main.js
function startNetworkingModule() {
  return new Promise((resolve, reject) => {
    const executablePath = !app.isPackaged
      ? path.join(__dirname, '../cpp/bin/PeerBridgeNet.exe')
      : path.join(__basePackagePath, "..", 'app.asar.unpacked', 'cpp/bin/PeerBridgeNet.exe');
    console.log("IS DEV FROM GRPC: ", app.isPackaged);
    console.log("RESOURCES PATH ", process.resourcesPath);

    // const windowsStyle = isDev ? 'Normal' : 'Hidden';
    const windowsStyle = 'Normal' // Change after testing properly

    console.log(`Starting networking module from: ${executablePath}`);

    const psArgs = [
      '-NoProfile',
      '-Command',
      `Start-Process -FilePath '${executablePath}' -Verb RunAs -WindowStyle ${windowsStyle}`
    ];

    // Spawn PowerShell. `windowsHide: false` ensures the PS window is not suppressed.
    networkProcess = spawn('powershell.exe', psArgs, {
      windowsHide: !false,
      stdio: 'ignore'
    });

    networkProcess.on('error', (err) => {
      console.error('Failed to spawn elevated process:', err);
      reject(err);
    });

    // As soon as PowerShell is successfully spawned, resolve.
    // (The elevated helper will appear in its own window once the user clicks "Yes".)
    networkProcess.on('spawn', () => {
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

function stopProcess() {
  return new Promise((resolve, reject) => {
    const client = connectGrpcClient();
    client.stopProcess({force: false}, (error, response) => {
      resolve(response);
    });
  });
}

// Start connection with peers
function startConnection(peerInfo, selfIndex, shouldFail = false) {
  return new Promise((resolve, reject) => {
    const client = connectGrpcClient();
    
    console.log("Calling startConnection with:", {
      peer_info: peerInfo,
      self_index: selfIndex,
      should_fail: shouldFail
    });
    
    client.startConnection({
      peer_info: peerInfo,
      self_index: selfIndex,
      should_fail: shouldFail
    }, (error, response) => {
      if (error) {
        console.error('Error starting connection:', error);
        reject(error);
        return;
      }
      
      console.log("StartConnection response:", response);
      resolve({
        success: response.success,
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
async function cleanup() {
  console.log("Stopping C++ module");
  const response = await stopProcess();
  console.log("C++ module stopped with response:", response);
  networkProcess.kill();
  grpcClient = null;
  networkProcess = null;
}

module.exports = {
  initializeNetworking,
  getStunInfo,
  startConnection,
  cleanup
};
