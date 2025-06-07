import { io } from 'socket.io-client';
import axiosInstance from './axios';

// Create a socket instance - connect to same host as API
const backend = "https://striking-washer-hist-range.trycloudflare.com";
const socket = io(backend, {
  autoConnect: false, // Don't connect automatically, we'll do it after auth
  transports: ['websocket', 'polling'], // Try WebSocket first, fall back to polling
  reconnectionAttempts: 10, // Increase from 5
  reconnectionDelay: 1000, // Start with 1s delay
  reconnectionDelayMax: 5000, // Max 5s between attempts
  timeout: 20000, // Increase timeout for complex operations
  pingTimeout: 15000, // Longer ping timeout
  pingInterval: 10000, // More frequent pings
  upgrade: true,
  rememberUpgrade: true
});

// Event listeners
socket.on('connect', () => {
  console.log('Socket connected successfully');
  
  // TODO: Maybe add some code here later
});

socket.on('connected', (data) => {
  console.log('Server confirms connection:', data);
});

socket.on('auth_error', (error) => {
  console.error('Authentication error:', error);

  // TODO: Maybe add some code here later
});

socket.on('disconnect', () => {
  console.log('Socket disconnected');
});

/**
 * Initialize socket connection
 * Called after successful login
 */
export const initializeSocket = () => {
  const userData = JSON.parse(localStorage.getItem('user'));
  const token = userData.access_token;
  console.log(token);
  socket.auth = { token };   // attach auth payload
  socket.connect();
};

export const stopSocket = () => {
  socket.disconnect();
};

/**
 * Join a socket room
 * @param {string} roomId - ID of the room to join
 */
export const joinRoom = (roomId) => {
  socket.emit('join_room', { room: roomId });
};

/**
 * Leave a socket room
 * @param {string} roomId - ID of the room to leave
 */
export const leaveRoom = (roomId) => {
  socket.emit('leave_room', { room: roomId });
};

/**
 * Send a message to a room
 * @param {string} roomId - ID of the room to message
 * @param {string} message - The message to send
 */
export const sendMessage = (roomId, message) => {
  socket.emit('message', { room: roomId, message });
};

/**
 * Join a lobby's socket room to receive real-time updates
 * @param {string} lobbyId - ID of the lobby to join
 */
export const joinLobbyRoom = (lobbyId) => {
  socket.emit('join_lobby_room', { lobby_id: lobbyId });
};

/**
 * Leave a lobby's socket room
 * @param {string} lobbyId - ID of the lobby to leave
 */
export const leaveLobbyRoom = (lobbyId) => {
  socket.emit('leave_lobby_room', { lobby_id: lobbyId });
};

/**
 * Listen for lobby starting events
 * @param {Function} callback - Callback to handle the event
 * @returns {Function} - Function to remove the listener
 */
export const onLobbyStarting = (callback) => {
  socket.on('lobby_starting', callback);
  return () => socket.off('lobby_starting', callback);
};

/**
 * Listen for lobby stopping events
 * @param {Function} callback - Callback to handle the event
 * @returns {Function} - Function to remove the listener
 */
export const onLobbyStopping = (callback) => {
  socket.on('lobby_stopping', callback);
  return () => socket.off('lobby_stopping', callback);
};

/**
 * Get current lobby data from the server
 * @param {string} lobbyId - ID of the lobby
 * @returns {Promise} - Resolves with lobby data
 */
export const getLobbyData = (lobbyId) => {
  return new Promise((resolve, reject) => {
    const onLobbyData = (data) => {
      socket.off('lobby_data', onLobbyData);
      socket.off('lobby_error', onLobbyError);
      resolve(data);
    };

    const onLobbyError = (error) => {
      socket.off('lobby_data', onLobbyData);
      socket.off('lobby_error', onLobbyError);
      reject(error);
    };

    socket.on('lobby_data', onLobbyData);
    socket.on('lobby_error', onLobbyError);

    socket.emit('get_lobby_data', { lobby_id: lobbyId });
  });
};

// Export the socket instance for direct access if needed
export default socket;
