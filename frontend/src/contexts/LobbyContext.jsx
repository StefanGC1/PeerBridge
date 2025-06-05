import React, { createContext, useState, useContext, useEffect } from 'react';
import socket from '../lib/socket';
import { joinLobbyRoom, leaveLobbyRoom } from '../lib/socket';

// Create the context
const LobbyContext = createContext(null);

// Create a provider component
export function LobbyProvider({ children }) {
  const [activeLobby, setActiveLobby] = useState(null);

  useEffect(() => {
    if (!activeLobby) return;

    // Listen for lobby updates
    const handleLobbyUpdated = (updatedLobby) => {
      if (activeLobby && updatedLobby.id === activeLobby.id) {
        setActiveLobby(updatedLobby);
      }
    };

    // Listen for lobby deletion
    const handleLobbyDeleted = (data) => {
      if (activeLobby && data.lobby_id === activeLobby.id) {
        setActiveLobby(null);
      }
    };

    // Join the socket room for this lobby to receive real-time updates
    joinLobbyRoom(activeLobby.id);
    socket.on('lobby_updated', handleLobbyUpdated);
    socket.on('lobby_deleted', handleLobbyDeleted);

    return () => {
      // Leave the socket room when the component unmounts or the active lobby changes
      leaveLobbyRoom(activeLobby.id);
      socket.off('lobby_updated', handleLobbyUpdated);
      socket.off('lobby_deleted', handleLobbyDeleted);
    };
  }, [activeLobby?.id]);

  // The context value that will be supplied to any descendants of this provider
  const contextValue = {
    activeLobby,
    setActiveLobby,
  };

  return (
    <LobbyContext.Provider value={contextValue}>
      {children}
    </LobbyContext.Provider>
  );
}

// Custom hook for consuming the context
export const UseLobby = () => {
  const context = useContext(LobbyContext);
  if (!context) {
    throw new Error('UseLobby must be used within a LobbyProvider');
  }
  return context;
} 