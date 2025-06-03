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
    // sudoPrompt.exec(`"${executablePath}"`, options, (error, stdout, stderr) => {
    //   if (error) {
    //     console.error('Error starting network module:', error);
    //     reject(error);
    //     return;
    //   }
      
    //   console.log('Network module started successfully');
    //   resolve();
    // });

    // const windowsStyle = isDev ? 'Normal' : 'Hidden';

    // const psArgs = [
    //   '-NoProfile',
    //   '-Command',
    //   `Start-Process -FilePath '${executablePath}' -Verb RunAs -WindowStyle ${windowsStyle}`
    // ];

    // // Spawn PowerShell. `windowsHide: false` ensures the PS window is not suppressed.
    // networkProcess = spawn('powershell.exe', psArgs, {
    //   windowsHide: !false,
    //   stdio: 'ignore'
    // });

    // networkProcess.on('error', (err) => {
    //   console.error('Failed to spawn elevated process:', err);
    //   reject(err);
    // });

    // // As soon as PowerShell is successfully spawned, resolve.
    // // (The elevated helper will appear in its own window once the user clicks "Yes".)
    // networkProcess.on('spawn', () => {
    //   resolve();
    // });
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
  grpcClient.close();
  networkProcess.kill();
  grpcClient = null;
  networkProcess = null;
}

module.exports = {
  initializeNetworking,
  getStunInfo,
  cleanup
};
