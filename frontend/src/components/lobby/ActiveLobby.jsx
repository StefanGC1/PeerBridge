import React, { useState, useEffect } from 'react';
import { Users, Copy, X, Settings } from 'lucide-react';
import { updateLobby, leaveLobby } from '../../lib/api';
import socket from '../../lib/socket';
import { leaveLobbyRoom } from '../../lib/socket';
import { useLobby } from '../../contexts/LobbyContext';

function ActiveLobby({ lobbyData }) {
  const { activeLobby, setActiveLobby } = useLobby();
  const [lobby, setLobby] = useState(lobbyData || activeLobby);
  const [copied, setCopied] = useState(false);
  const [showSettings, setShowSettings] = useState(false);
  const [lobbyName, setLobbyName] = useState(lobby?.name || '');
  const [maxPlayers, setMaxPlayers] = useState(lobby?.max_players || 4);
  const [error, setError] = useState('');
  
  const userData = JSON.parse(localStorage.getItem('user') || '{}');
  const isHost = lobby?.host === userData.user_id;

  useEffect(() => {
    // Update local state when the global active lobby changes
    if (activeLobby) {
      setLobby(activeLobby);
      setLobbyName(activeLobby.name);
      setMaxPlayers(activeLobby.max_players);
    }
  }, [activeLobby]);

  useEffect(() => {
    // Listen for lobby updates
    const handleLobbyUpdated = (updatedLobby) => {
      if (lobby && updatedLobby.id === lobby.id) {
        setLobby(updatedLobby);
        setLobbyName(updatedLobby.name);
        setMaxPlayers(updatedLobby.max_players);
      }
    };

    socket.on('lobby_updated', handleLobbyUpdated);

    return () => {
      socket.off('lobby_updated', handleLobbyUpdated);
    };
  }, [lobby?.id]);

  const handleCopyId = () => {
    if (lobby) {
      navigator.clipboard.writeText(lobby.id);
      setCopied(true);
      setTimeout(() => setCopied(false), 2000);
    }
  };

  const handleLeave = async () => {
    try {
      if (activeLobby) {
        // Call the API to leave the lobby
        leaveLobbyRoom(activeLobby.id);
        await leaveLobby(activeLobby.id);
        setActiveLobby(null);
      }
    } catch (err) {
      console.error('Error leaving lobby:', err);
    }
  };

  const handleUpdateLobby = async (e) => {
    e.preventDefault();
    setError('');

    if (!lobby) return;

    try {
      // Call the API to update the lobby
      const updatedLobby = await updateLobby(lobby.id, {
        name: lobbyName,
        max_players: maxPlayers
      });
      
      // Update local state with the response data
      setLobby(updatedLobby);
      setShowSettings(false);
    } catch (err) {
      setError(err.response?.data?.error || 'Failed to update lobby');
    }
  };

  // If no lobby data is available, show a loading state or return null
  if (!lobby) {
    return <div className="text-center py-10">Loading lobby...</div>;
  }

  return (
    <div className="bg-card border border-border rounded-lg overflow-hidden">
      <div className="p-6">
        <div className="flex justify-between items-center mb-6">
          <h2 className="text-xl font-semibold">{lobby.name}</h2>
          <div className="flex space-x-2">
            {isHost && (
              <button 
                onClick={() => setShowSettings(!showSettings)}
                className="p-2 hover:bg-accent rounded-full"
                title="Lobby Settings"
              >
                <Settings size={20} />
              </button>
            )}
            <button 
              onClick={handleLeave}
              className="p-2 hover:bg-destructive/10 hover:text-destructive rounded-full"
              title="Leave Lobby"
            >
              <X size={20} />
            </button>
          </div>
        </div>

        {showSettings && isHost && (
          <div className="mb-6 p-4 bg-background rounded-lg">
            <h3 className="text-sm font-medium mb-3">Lobby Settings</h3>
            
            {error && (
              <div className="bg-destructive/10 text-destructive text-sm p-3 rounded mb-4">
                {error}
              </div>
            )}
            
            <form onSubmit={handleUpdateLobby}>
              <div className="space-y-3">
                <div>
                  <label htmlFor="lobbyName" className="block text-xs text-muted-foreground mb-1">
                    Lobby Name
                  </label>
                  <input
                    type="text"
                    id="lobbyName"
                    value={lobbyName}
                    onChange={(e) => setLobbyName(e.target.value)}
                    className="w-full px-3 py-2 text-sm bg-card border border-input rounded-md"
                  />
                </div>
                
                <div>
                  <label htmlFor="maxPlayers" className="block text-xs text-muted-foreground mb-1">
                    Max Players
                  </label>
                  <select
                    id="maxPlayers"
                    value={maxPlayers}
                    onChange={(e) => setMaxPlayers(Number(e.target.value))}
                    className="w-full px-3 py-2 text-sm bg-card border border-input rounded-md"
                  >
                    <option value="2">2 Players</option>
                    <option value="3">3 Players</option>
                    <option value="4">4 Players</option>
                    <option value="6">6 Players</option>
                    <option value="8">8 Players</option>
                  </select>
                </div>
                
                <div className="flex justify-end pt-2">
                  <button
                    type="submit"
                    className="px-3 py-1 text-sm bg-primary text-primary-foreground rounded-md"
                  >
                    Save Changes
                  </button>
                </div>
              </div>
            </form>
          </div>
        )}

        <div className="mb-6">
          <div 
            className="flex items-center justify-between p-3 bg-background rounded-lg cursor-pointer hover:bg-accent/50"
            onClick={handleCopyId}
          >
            <div className="text-sm">
              <span className="text-muted-foreground mr-2">Lobby ID:</span>
              <span className="font-mono">{lobby.id}</span>
            </div>
            <div className="flex items-center">
              {copied ? (
                <span className="text-xs text-primary mr-2">Copied!</span>
              ) : (
                <span className="text-xs text-muted-foreground mr-2">Click to copy</span>
              )}
              <Copy size={16} className="text-muted-foreground" />
            </div>
          </div>
        </div>

        <div>
          <h3 className="text-sm font-medium flex items-center mb-2">
            <Users size={16} className="mr-2" />
            Players ({lobby.members.length}/{lobby.max_players})
          </h3>
          <div className="space-y-2">
            {/* This would ideally show player names, but we'll use IDs for now */}
            {lobby.members.map((memberId) => (
              <div 
                key={memberId}
                className="flex items-center justify-between p-2 rounded-lg hover:bg-accent/50"
              >
                <div className="flex items-center">
                  <div className="w-8 h-8 rounded-full bg-primary/10 text-primary flex items-center justify-center mr-3">
                    {memberId.slice(0, 2).toUpperCase()}
                  </div>
                  <div className="font-mono text-sm truncate">
                    {memberId}
                    {memberId === lobby.host && (
                      <span className="ml-2 text-xs bg-primary/10 text-primary px-2 py-0.5 rounded-full">
                        Host
                      </span>
                    )}
                  </div>
                </div>
              </div>
            ))}
          </div>
        </div>
      </div>
      
      <div className="bg-accent/50 p-4 flex justify-between items-center border-t border-border">
        <div className="text-sm text-muted-foreground">
          Waiting for players...
        </div>
        {isHost && (
          <button 
            className="px-4 py-2 bg-primary text-primary-foreground rounded-md text-sm"
            disabled={lobby.members.length < 2}
          >
            Start Game
          </button>
        )}
      </div>
    </div>
  );
}

export default ActiveLobby; 