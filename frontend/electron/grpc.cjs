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
        publicKey: response.public_key,
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
function startConnection(peerInfoArray, selfIndex, shouldFail = false) {
  return new Promise((resolve, reject) => {
    const client = connectGrpcClient();
    
    // Convert the array of peer info to the new format with PeerInfo objects
    const peers = peerInfoArray.map(peer => {
      const hex = Array.isArray(peer.public_key)
                      ? Buffer.from(peer.public_key).toString('hex')
                      : peer.public_key;
      return {
        stun_info: peer.stun_info,
        public_key: hex
                    ? Buffer.from(hex, 'hex')
                    : Buffer.alloc(0)
      };
    });
    
    console.log("Calling startConnection with:", {
      peers: peers.map(p => ({ stun_info: p.stun_info, has_key: p.public_key.length > 0 })),
      self_index: selfIndex,
      should_fail: shouldFail
    });
    
    client.startConnection({
      peers: peers,
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

function stopConnection() {
  return new Promise((resolve, reject) => {
    const client = connectGrpcClient();
    client.stopConnection({}, (error, response) => {
      if (error) {
        console.error('Error stopping connection:', error);
        reject(error);
        return;
      }
      console.log("StopConnection response:", response);
      resolve(response);
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
  if (!networkProcess) {
    console.log("No network process to cleanup");
    return;
  }
  console.log("Stopping C++ module");
  const response = await stopProcess();
  // Give C++ module time to exit gracefully
  await new Promise(resolve => setTimeout(resolve, 1000));
  grpc.closeClient(grpcClient);
  networkProcess.kill();
  networkProcess = null;
  console.log("C++ module stopped with response:", response);
  grpcClient = null;
}

module.exports = {
  initializeNetworking,
  getStunInfo,
  startConnection,
  stopConnection,
  cleanup
};
