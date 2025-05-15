import axiosInstance from "./axios";

export async function login(username, password) {
  try {
    const response = await axiosInstance.post("/api/login", {username, password});
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