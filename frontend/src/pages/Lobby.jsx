import React, { useState, useEffect } from 'react';
import { Plus, LogIn, Clock, Users } from 'lucide-react';
import CreateLobbyModal from '../components/lobby/CreateLobbyModal';
import JoinLobbyModal from '../components/lobby/JoinLobbyModal';
import ActiveLobby from '../components/lobby/ActiveLobby';
import socket from '../lib/socket';
import { useLobby } from '../contexts/LobbyContext';

function Lobby() {
  const [showCreateModal, setShowCreateModal] = useState(false);
  const [showJoinModal, setShowJoinModal] = useState(false);
  const [onlinePlayers, setOnlinePlayers] = useState(0);
  
  // Use the lobby context instead of local state
  const { activeLobby, joinLobbyState, leaveLobbyState } = useLobby();

  useEffect(() => {
    // Listen for online player count updates
    socket.on('online_count', (data) => {
      setOnlinePlayers(data.count);
    });

    return () => {
      socket.off('online_count');
    };
  }, []);

  const handleLobbyCreated = (lobbyData) => {
    joinLobbyState(lobbyData);
    setShowCreateModal(false);
  };

  const handleLobbyJoined = (lobbyData) => {
    joinLobbyState(lobbyData);
    setShowJoinModal(false);
  };

  return (
    <div className="h-full flex flex-col">
      <div className="bg-card border-b border-border p-6">
        <div className="flex justify-between items-center">
          <h1 className="text-2xl font-bold text-foreground">Lobby</h1>
        </div>
      </div>
      
      <div className="flex-1 overflow-auto p-6">
        {activeLobby ? (
          <ActiveLobby 
            lobbyData={activeLobby} 
            onLeaveLobby={leaveLobbyState} 
          />
        ) : (
          <div className="flex flex-col items-center justify-center h-full gap-6">
            <h2 className="text-xl font-medium text-foreground mb-2">What would you like to do?</h2>
            
            <div className="grid grid-cols-1 md:grid-cols-2 gap-6 w-full max-w-2xl">
              <button
                onClick={() => setShowCreateModal(true)}
                className="flex flex-col items-center justify-center bg-card border border-border hover:border-primary rounded-lg p-8 transition-colors gap-3 group"
              >
                <div className="w-16 h-16 rounded-full bg-primary/10 flex items-center justify-center text-primary group-hover:bg-primary group-hover:text-primary-foreground transition-colors">
                  <Plus size={32} />
                </div>
                <h3 className="text-lg font-medium text-foreground">Create Lobby</h3>
                <p className="text-sm text-muted-foreground text-center">
                  Create a new game session and invite friends
                </p>
              </button>
              
              <button
                onClick={() => setShowJoinModal(true)}
                className="flex flex-col items-center justify-center bg-card border border-border hover:border-primary rounded-lg p-8 transition-colors gap-3 group"
              >
                <div className="w-16 h-16 rounded-full bg-primary/10 flex items-center justify-center text-primary group-hover:bg-primary group-hover:text-primary-foreground transition-colors">
                  <LogIn size={32} />
                </div>
                <h3 className="text-lg font-medium text-foreground">Join Lobby</h3>
                <p className="text-sm text-muted-foreground text-center">
                  Enter a lobby code to join an existing game
                </p>
              </button>
            </div>
          </div>
        )}
      </div>
      
      <div className="bg-card border-t border-border p-4 text-sm text-muted-foreground">
        <div className="flex items-center justify-between">
          <span className="flex items-center gap-1">
            <Clock size={16} /> Last active: {new Date().toLocaleTimeString()}
          </span>
          <span className="flex items-center gap-1">
            <Users size={16} /> Online players: {onlinePlayers}
          </span>
        </div>
      </div>

      {showCreateModal && (
        <CreateLobbyModal 
          onClose={() => setShowCreateModal(false)}
          onLobbyCreated={handleLobbyCreated}
        />
      )}

      {showJoinModal && (
        <JoinLobbyModal 
          onClose={() => setShowJoinModal(false)}
          onLobbyJoined={handleLobbyJoined}
        />
      )}
    </div>
  );
}

export default Lobby;