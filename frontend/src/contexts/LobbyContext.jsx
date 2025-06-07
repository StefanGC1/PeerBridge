import React, { createContext, useState, useContext, useEffect, useRef } from 'react';
import socket from '../lib/socket';
import { joinLobbyRoom, leaveLobbyRoom, onLobbyStarting, onLobbyStopping } from '../lib/socket';
import { getUsersBatch, getPeerInfo } from '../lib/api';
import { startConnectionWithPeers } from '../lib/networking';

// Helper function to check if two arrays have the same elements
const areArraysEqual = (a, b) => {
  if (a === b) return true;
  if (!a || !b) return false;
  if (a.length !== b.length) return false;
  
  // Make copies to sort without modifying originals
  const sortedA = [...a].sort();
  const sortedB = [...b].sort();
  
  return sortedA.every((val, idx) => val === sortedB[idx]);
};

// Create the context
const LobbyContext = createContext(null);

// Create a provider component
export function LobbyProvider({ children }) {
  const [activeLobby, setActiveLobby] = useState(null);
  const [usernameMap, setUsernameMap] = useState({});
  const [loadingUsernames, setLoadingUsernames] = useState(false);
  const previousMembersRef = useRef([]);

  // Join/leave lobby socket room when active lobby changes
  useEffect(() => {
    if (!activeLobby) return;
    
    // Join the socket room for this lobby to receive real-time updates
    joinLobbyRoom(activeLobby.id);
    
    return () => {
      // Leave the socket room when the component unmounts or the active lobby changes
      leaveLobbyRoom(activeLobby.id);
    };
  }, [activeLobby?.id]);
  
  // Set up socket event listeners for lobby updates
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
        setUsernameMap({});
        previousMembersRef.current = [];
      }
    };
    
    // Listen for lobby starting
    const removeStartingListener = onLobbyStarting((updatedLobby) => {
      if (updatedLobby.id === activeLobby.id) {
        console.log('Lobby is starting:', updatedLobby);
        setActiveLobby(updatedLobby);
        
        // Request peer info when lobby is starting and initiate connection immediately
        getPeerInfo(updatedLobby.id)
          .then(peerInfo => {
            console.log('Peer connection info received:', peerInfo);
            
            // Immediately attempt connection with the C++ module
            return startConnectionWithPeers(
              peerInfo.peer_info,
              peerInfo.self_index,
              true // shouldFail - set to false by default
            );
          })
          .then(result => {
            console.log('Connection result:', result);
            
            if (result.success) {
              console.log('Connection successful, emitting lobby_user_connected');
              socket.emit('lobby_user_connected', { lobby_id: updatedLobby.id });
            } else {
              console.error('Connection failed:', result.errorMessage);
              socket.emit('lobby_user_failure', { 
                lobby_id: updatedLobby.id,
                error: result.errorMessage
              });
            }
          })
          .catch(error => {
            console.error('Error in connection process:', error);
            socket.emit('lobby_user_failure', { 
              lobby_id: updatedLobby.id,
              error: error.message || 'Unknown error'
            });
          });
      }
    });
    
    // Listen for lobby stopping
    const removeStoppingListener = onLobbyStopping((updatedLobby) => {
      if (updatedLobby.id === activeLobby.id) {
        console.log('Lobby is stopping:', updatedLobby);
        setActiveLobby(updatedLobby);
      }
    });

    socket.on('lobby_updated', handleLobbyUpdated);
    socket.on('lobby_deleted', handleLobbyDeleted);

    return () => {
      socket.off('lobby_updated', handleLobbyUpdated);
      socket.off('lobby_deleted', handleLobbyDeleted);
      removeStartingListener();
      removeStoppingListener();
    };
  }, [activeLobby?.id]);

  // Fetch usernames only when the member list actually changes
  useEffect(() => {
    if (!activeLobby || !activeLobby.members || activeLobby.members.length === 0) return;

    const currentMembers = activeLobby.members;
    if (areArraysEqual(currentMembers, previousMembersRef.current)) {
      // Members list hasn't changed, no need to fetch usernames again
      return;
    }
    
    // Update the reference to current members
    previousMembersRef.current = currentMembers;
    
    // Fetch usernames for the updated member list
    const fetchUsernames = async () => {
      setLoadingUsernames(true);
      try {
        const userMap = await getUsersBatch(currentMembers);
        setUsernameMap(userMap);
      } catch (err) {
        console.error('Error fetching usernames:', err);
      } finally {
        setLoadingUsernames(false);
      }
    };
    
    fetchUsernames();
  }, [activeLobby?.members]);

  // The context value that will be supplied to any descendants of this provider
  const contextValue = {
    activeLobby,
    setActiveLobby,
    usernameMap,
    loadingUsernames
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