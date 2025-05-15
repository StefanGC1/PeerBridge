import { io } from 'socket.io-client';
import axiosInstance from './axios';

// Create a socket instance - connect to same host as API
const socket = io(axiosInstance.defaults.baseURL, {
  autoConnect: false, // Don't connect automatically, we'll do it after auth
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
 * Join a game room
 * @param {string} roomId - ID of the room to join
 */
export const joinRoom = (roomId) => {
  socket.emit('join', { room: roomId });
};

/**
 * Leave a game room
 * @param {string} roomId - ID of the room to leave
 */
export const leaveRoom = (roomId) => {
  socket.emit('leave', { room: roomId });
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
 * Create a new lobby
 * @param {string} name - Name of the lobby
 * @param {number} maxPlayers - Maximum players allowed in the lobby
 * @returns {Promise} - Resolves when lobby is created
 */
export const createLobby = (name, maxPlayers = 4) => {
  return new Promise((resolve, reject) => {
    const onLobbyCreated = (data) => {
      socket.off('lobby_created', onLobbyCreated);
      socket.off('lobby_error', onLobbyError);
      resolve(data);
    };

    const onLobbyError = (error) => {
      socket.off('lobby_created', onLobbyCreated);
      socket.off('lobby_error', onLobbyError);
      reject(error);
    };

    socket.on('lobby_created', onLobbyCreated);
    socket.on('lobby_error', onLobbyError);

    socket.emit('create_lobby', { name, max_players: maxPlayers });
  });
};

/**
 * Join an existing lobby by ID
 * @param {string} lobbyId - ID of the lobby to join
 * @returns {Promise} - Resolves when lobby is joined
 */
export const joinLobby = (lobbyId) => {
  return new Promise((resolve, reject) => {
    const onLobbyJoined = (data) => {
      socket.off('lobby_joined', onLobbyJoined);
      socket.off('lobby_error', onLobbyError);
      resolve(data);
    };

    const onLobbyError = (error) => {
      socket.off('lobby_joined', onLobbyJoined);
      socket.off('lobby_error', onLobbyError);
      reject(error);
    };

    socket.on('lobby_joined', onLobbyJoined);
    socket.on('lobby_error', onLobbyError);

    socket.emit('join_lobby', { lobby_id: lobbyId });
  });
};

/**
 * Leave the current lobby
 * @param {string} lobbyId - ID of the lobby to leave
 * @returns {Promise} - Resolves when lobby is left
 */
export const leaveLobby = (lobbyId) => {
  return new Promise((resolve, reject) => {
    const onLobbyLeft = (data) => {
      socket.off('lobby_left', onLobbyLeft);
      socket.off('lobby_error', onLobbyError);
      resolve(data);
    };

    const onLobbyError = (error) => {
      socket.off('lobby_left', onLobbyLeft);
      socket.off('lobby_error', onLobbyError);
      reject(error);
    };

    socket.on('lobby_left', onLobbyLeft);
    socket.on('lobby_error', onLobbyError);

    socket.emit('leave_lobby', { lobby_id: lobbyId });
  });
};

/**
 * Update lobby settings (host only)
 * @param {string} lobbyId - ID of the lobby to update
 * @param {object} settings - Settings to update
 * @returns {Promise} - Resolves when lobby is updated
 */
export const updateLobby = (lobbyId, settings) => {
  return new Promise((resolve, reject) => {
    const onLobbyUpdated = (data) => {
      socket.off('lobby_update_success', onLobbyUpdated);
      socket.off('lobby_error', onLobbyError);
      resolve(data);
    };

    const onLobbyError = (error) => {
      socket.off('lobby_update_success', onLobbyUpdated);
      socket.off('lobby_error', onLobbyError);
      reject(error);
    };

    socket.on('lobby_update_success', onLobbyUpdated);
    socket.on('lobby_error', onLobbyError);

    socket.emit('update_lobby', { lobby_id: lobbyId, ...settings });
  });
};

// Export the socket instance for direct access if needed
export default socket;
