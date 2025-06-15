import React, { useEffect, useState } from 'react';
import { useNavigate } from 'react-router-dom';
import Router from './routes';
import { useTheme } from './contexts/ThemeContext';
import LoadingScreen from './components/LoadingScreen';
import { tokenManager } from './lib/token';
import { refreshToken } from './lib/api';
import { initializeSocket } from './lib/socket';

function App() {
  const { theme } = useTheme();
  const [isInitializing, setIsInitializing] = useState(true);
  const [initMessage, setInitMessage] = useState('Initializing...');
  
  useEffect(() => {
    // Initialize app and check for refresh token
    const initializeApp = async () => {
      try {
        setInitMessage('Checking authentication...');
        const startTime = Date.now();

        const storedRefreshToken = await tokenManager.getRefreshToken();

        if (storedRefreshToken) {
          setInitMessage('Initializing networking...');
          
          // Wait 2 seconds for C++ module to initialize
          await new Promise(resolve => setTimeout(resolve, 2000));
          
          setInitMessage('Authenticating...');
          try {
            const userData = await refreshToken(storedRefreshToken);
            tokenManager.storeUserData(userData);
            setInitMessage('Connecting...');

            initializeSocket();
            setInitMessage('Ready!');
            // Wait for minimum loading time
            const elapsed = Date.now() - startTime;
            if (elapsed < 2000) {
              await new Promise(resolve => setTimeout(resolve, 2000 - elapsed));
            }

            // Navigate to lobby
            window.location.hash = '/app/lobby';

          } catch (error) {
            console.warn('Failed to refresh token:', error);
            // Remove invalid refresh token
            await tokenManager.removeRefreshToken();
            tokenManager.clearUserData();
          }
        }
        
        // Ensure minimum loading time
        const elapsed = Date.now() - startTime;
        if (elapsed < 2000) {
          await new Promise(resolve => setTimeout(resolve, 2000 - elapsed));
        }
        
      } catch (error) {
        console.error('App initialization error:', error);
      } finally {
        setIsInitializing(false);
      }
    };

    // Initialize any global listeners or setup
    const handleOnlineStatus = () => {
      console.log('Network status changed');
    };

    window.addEventListener('online', handleOnlineStatus);
    window.addEventListener('offline', handleOnlineStatus);

    initializeApp();

    return () => {
      window.removeEventListener('online', handleOnlineStatus);
      window.removeEventListener('offline', handleOnlineStatus);
    };
  }, []);

  return (
    <div className={`app ${theme}`}>
      <Router />
      {isInitializing && <LoadingScreen message={initMessage} />}
    </div>
  );
}

export default App;