const SERVICE_NAME = 'PeerBridge';
const REFRESH_TOKEN_ACCOUNT = 'refresh_token';

export const tokenManager = {
  async storeRefreshToken(refreshToken) {
    try {
      const result = await window.electron.keytar.setPassword(
        SERVICE_NAME,
        REFRESH_TOKEN_ACCOUNT,
        refreshToken
      );
      return result.success;
    } catch (error) {
      console.error('Error storing refresh token:', error);
      return false;
    }
  },

  async getRefreshToken() {
    try {
      const refreshToken = await window.electron.keytar.getPassword(
        SERVICE_NAME,
        REFRESH_TOKEN_ACCOUNT
      );
      return refreshToken;
    } catch (error) {
      console.error('Error retrieving refresh token:', error);
      return null;
    }
  },

  async removeRefreshToken() {
    try {
      const result = await window.electron.keytar.deletePassword(
        SERVICE_NAME,
        REFRESH_TOKEN_ACCOUNT
      );
      return result.success;
    } catch (error) {
      console.error('Error removing refresh token:', error);
      return false;
    }
  },

  clearUserData() {
    localStorage.removeItem('user');
  },

  storeUserData(userData) {
    localStorage.setItem('user', JSON.stringify(userData));
  },

  getUserData() {
    const userData = localStorage.getItem('user');
    return userData ? JSON.parse(userData) : null;
  }
}; 