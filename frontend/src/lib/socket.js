import { io } from 'socket.io-client';
import axiosInstance from './axios';

// Create a socket instance - connect to same host as API
const backend = "https://b95d-86-125-92-157.ngrok-free.app";
const socket = io(backend, {
  autoConnect: false, // Don't connect automatically, we'll do it after auth
  transports: ['websocket', 'polling'], // Try WebSocket first, fall back to polling
  extraHeaders: {
    'ngrok-skip-browser-warning': 'true'
  },
  reconnectionAttempts: 5,
  timeout: 10000, // Increase timeout for ngrok
  upgrade: true, // Try to upgrade to WebSocket if possible
  rememberUpgrade: true
});

// Event listeners
socket.on('connect', () => {
  console.log('Socket connected successfully');
  
  // Get token from localStorage
  const userData = JSON.parse(localStorage.getItem('user'));
  if (userData && userData.access_token) {
    // Authenticate after connection
    socket.emit('authenticate', { token: userData.access_token });
  }
});

socket.on('connected', (data) => {
  console.log('Server confirms connection:', data);
});

socket.on('authenticated', (data) => {
  console.log('Authentication successful:', data);
  
  // Join user's personal room after authentication
  const userData = JSON.parse(localStorage.getItem('user'));
  if (userData && userData.user_id) {
    // Refactorize this
    joinRoom(`user:${userData.user_id}`);
    console.log(`Joined personal room: user:${userData.user_id}`);
  }
});

socket.on('auth_error', (error) => {
  console.error('Authentication error:', error);
});

socket.on('disconnect', () => {
  console.log('Socket disconnected');
});

/**
 * Initialize socket connection
 * Called after successful login
 */
export const initializeSocket = () => {
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
