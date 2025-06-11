/**
 * Start a connection with peers using the C++ networking module
 * @param {string[]} peerInfo - Array of peer connection strings (ip:port)
 * @param {number} selfIndex - Index of self in the peer_info list
 * @param {boolean} shouldFail - Testing flag to simulate failure
 * @returns {Promise<{success: boolean, errorMessage: string}>} - Connection result
 */
export const startConnectionWithPeers = async (peerInfo, selfIndex, shouldFail = false) => {
  try {
    if (!window.electron?.grpc?.startConnection) {
      console.error('gRPC startConnection not available');
      return { 
        success: false, 
        errorMessage: 'gRPC startConnection not available' 
      };
    }

    console.log('Starting connection with peers:', { peerInfo, selfIndex });
    const result = await window.electron.grpc.startConnection(peerInfo, selfIndex, shouldFail);
    console.log('Connection result:', result);
    
    return result;
  } catch (error) {
    console.error('Error starting connection with peers:', error);
    return { 
      success: false, 
      errorMessage: error.message || 'Unknown error starting connection' 
    };
  }
};

export const stopConnection = async () => {
  try {
    if (!window.electron?.grpc?.stopConnection) {
      console.error('gRPC stopConnection not available');
      return { success: false, errorMessage: 'gRPC stopConnection not available' };
    }

    console.log('Stopping connection');
    const result = await window.electron.grpc.stopConnection();
    console.log('Connection stopped:', result);
    return result;
  } catch (error) {
    console.error('Error stopping connection:', error);
    return { success: false, errorMessage: error.message || 'Unknown error stopping connection' };
  }
};