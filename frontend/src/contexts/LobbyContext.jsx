import React, { createContext, useState, useContext, useEffect } from 'react';
import socket from '../lib/socket';
import { leaveLobby } from '../lib/socket';

// Create the context
const LobbyContext = createContext(null);

// Create a provider component
export function LobbyProvider({ children }) {
  const [activeLobby, setActiveLobby] = useState(null);

  useEffect(() => {
    // Listen for lobby updates
    const handleLobbyUpdated = (updatedLobby) => {
      if (activeLobby && updatedLobby.id === activeLobby.id) {
        setActiveLobby(updatedLobby);
      }
    };

    socket.on('lobby_updated', handleLobbyUpdated);

    return () => {
      socket.off('lobby_updated', handleLobbyUpdated);
    };
  }, [activeLobby]);

  const joinLobbyState = (lobbyData) => {
    setActiveLobby(lobbyData);
  };

  const leaveLobbyState = async () => {
    if (activeLobby) {
      try {
        await leaveLobby(activeLobby.id);
      } catch (err) {
        console.error('Error leaving lobby:', err);
      }
    }
    setActiveLobby(null);
  };

  // The context value that will be supplied to any descendants of this provider
  const contextValue = {
    activeLobby,
    joinLobbyState,
    leaveLobbyState,
  };

  return (
    <LobbyContext.Provider value={contextValue}>
      {children}
    </LobbyContext.Provider>
  );
}

// Custom hook for consuming the context
export function useLobby() {
  const context = useContext(LobbyContext);
  if (!context) {
    throw new Error('useLobby must be used within a LobbyProvider');
  }
  return context;
} 