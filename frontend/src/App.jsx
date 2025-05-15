import React, { useEffect } from 'react';
import Router from './routes';
import { useTheme } from './contexts/ThemeContext';

// Initialize socket and global API listeners
import './lib/socket';

function App() {
  const { theme } = useTheme();
  
  useEffect(() => {
    // Initialize any global listeners or setup
    const handleOnlineStatus = () => {
      console.log('Network status changed');
      // You might want to update some state or trigger reconnection
    };

    window.addEventListener('online', handleOnlineStatus);
    window.addEventListener('offline', handleOnlineStatus);

    return () => {
      window.removeEventListener('online', handleOnlineStatus);
      window.removeEventListener('offline', handleOnlineStatus);
    };
  }, []);

  return (
    <div className={`app ${theme}`}>
      <Router />
    </div>
  );
}

export default App;