import axios from "axios";

const backend = "https://peerbridge-backend-demo.polandcentral.cloudapp.azure.com";
const backend2 = "http://localhost:5000";
const axiosInstance = axios.create({
  baseURL: backend, // Replace with your API base URL
  timeout: 10000, // Request timeout (optional)
  headers: {
    "Content-Type": "application/json"
  },
});

// Add a request interceptor to include the JWT token in requests
axiosInstance.interceptors.request.use(
  (config) => {
    const userData = JSON.parse(localStorage.getItem('user'));
    if (userData && userData.access_token) {
      config.headers.Authorization = `Bearer ${userData.access_token}`;
    }
    return config;
  },
  (error) => {
    return Promise.reject(error);
  }
);

export default axiosInstance;