import axiosInstance from "./axios";

export async function login(username, password) {
  try {
    // Get STUN info before login
    let stunInfo = { publicIp: '', publicPort: 0 };
    try {
      stunInfo = await window.electron.grpc.getStunInfo();
      console.log('Got STUN info:', stunInfo);
    } catch (stunError) {
      console.warn('Failed to get STUN info:', stunError);
      // Continue with login even if STUN info fails
    }
    
    // Add STUN info to login request
    const response = await axiosInstance.post("/api/login", {
      username, 
      password,
      public_ip: stunInfo.publicIp || '',
      public_port: stunInfo.publicPort || 0
    });
    
    console.log('Login response:', response);
    return response.data;
  } catch (error) {
    console.warn('Error logging in:', error);
    throw error;
  }
}

export async function register(email, password) {
//   const response = await fetch('/api/auth/login', {
//     method: 'POST',
//     headers: {
//       'Content-Type': 'application/json',
//     },
//     body: JSON.stringify({ email, password }),
//   });

//   if (!response.ok) {
//     throw new Error('Login failed');
//   }

//   const data = await response.json();
  const data = "mockedUserData";
  return data;
}

export async function getActiveServers() {
//   const response = await fetch('/api/auth/login', {
//     method: 'POST',
//     headers: {
//       'Content-Type': 'application/json',
//     },
//     body: JSON.stringify({ email, password }),
//   });

//   if (!response.ok) {
//     throw new Error('Login failed');
//   }

//   const data = await response.json();
  const data = "mockedUserData";
  return data;
}

// Friend-related API functions
export async function getFriends() {
  try {
    const response = await axiosInstance.get("/api/friends");
    return response.data.friends;
  } catch (error) {
    console.warn('Error fetching friends:', error);
    throw error;
  }
}

export async function addFriend(userId) {
  try {
    const response = await axiosInstance.post(`/api/friends/add/${userId}`);
    return response.data;
  } catch (error) {
    console.warn('Error adding friend:', error);
    throw error;
  }
}

export async function removeFriend(userId) {
  try {
    const response = await axiosInstance.delete(`/api/friends/remove/${userId}`);
    return response.data;
  } catch (error) {
    console.warn('Error removing friend:', error);
    throw error;
  }
}

export async function searchUsers(query) {
  try {
    const response = await axiosInstance.get(`/api/users/search?q=${query}`);
    return response.data.users;
  } catch (error) {
    console.warn('Error searching users:', error);
    throw error;
  }
}

// Lobby management API functions
export async function createLobby(name, maxPlayers = 4) {
  try {
    const response = await axiosInstance.post('/api/lobbies/create-lobby', {
      name,
      max_players: maxPlayers
    });
    return response.data;
  } catch (error) {
    console.warn('Error creating lobby:', error);
    throw error;
  }
}

export async function joinLobby(lobbyId) {
  try {
    const response = await axiosInstance.post(`/api/lobbies/join-lobby/${lobbyId}`);
    return response.data;
  } catch (error) {
    console.warn('Error joining lobby:', error);
    throw error;
  }
}

export async function leaveLobby(lobbyId) {
  try {
    const response = await axiosInstance.post(`/api/lobbies/leave-lobby/${lobbyId}`);
    return response.data;
  } catch (error) {
    console.warn('Error leaving lobby:', error);
    throw error;
  }
}

export async function updateLobby(lobbyId, settings) {
  try {
    const response = await axiosInstance.put(`/api/lobbies/update-lobby/${lobbyId}`, settings);
    return response.data;
  } catch (error) {
    console.warn('Error updating lobby:', error);
    throw error;
  }
}

export async function deleteLobby(lobbyId) {
  try {
    const response = await axiosInstance.post(`/api/lobbies/delete-lobby/${lobbyId}`);
    return response.data;
  } catch (error) {
    console.warn('Error deleting lobby:', error);
    throw error;
  }
}

export async function getUsersBatch(userIds) {
  try {
    const response = await axiosInstance.post('/api/users/batch', {
      user_ids: userIds
    });
    return response.data.users;
  } catch (error) {
    console.warn('Error getting users batch:', error);
    throw error;
  }
}