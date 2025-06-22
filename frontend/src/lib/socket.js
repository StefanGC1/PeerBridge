import { io } from 'socket.io-client';
import axiosInstance from './axios';

const backend = "https://peerbridge-backend-demo.polandcentral.cloudapp.azure.com";
const socket = io(backend, {
  autoConnect: false,
  transports: ['websocket', 'polling'],
  reconnectionAttempts: 10,
  reconnectionDelay: 1000,
  reconnectionDelayMax: 5000,
  timeout: 20000,
  pingTimeout: 15000,
  pingInterval: 10000,
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
  socket.auth = { token };
  socket.connect();
};

export const stopSocket = () => {
  socket.disconnect();
};

export const joinRoom = (roomId) => {
  socket.emit('join_room', { room: roomId });
};

export const leaveRoom = (roomId) => {
  socket.emit('leave_room', { room: roomId });
};

export const sendMessage = (roomId, message) => {
  socket.emit('message', { room: roomId, message });
};

export const joinLobbyRoom = (lobbyId) => {
  socket.emit('join_lobby_room', { lobby_id: lobbyId });
};

export const leaveLobbyRoom = (lobbyId) => {
  socket.emit('leave_lobby_room', { lobby_id: lobbyId });
};

export const onLobbyStarting = (callback) => {
  socket.on('lobby_starting', callback);
  return () => socket.off('lobby_starting', callback);
};

export const onLobbyStopping = (callback) => {
  socket.on('lobby_stopping', callback);
  return () => socket.off('lobby_stopping', callback);
};

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
