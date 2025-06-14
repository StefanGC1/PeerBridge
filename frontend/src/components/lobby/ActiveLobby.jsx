import React, { useState, useEffect } from 'react';
import { Users, Copy, X, Settings, Play, Square } from 'lucide-react';
import { updateLobby, leaveLobby, startLobby, stopLobby } from '../../lib/api';
import { leaveLobbyRoom } from '../../lib/socket';
import { UseLobby } from '../../contexts/LobbyContext';

// Status color mapping
const STATUS_COLORS = {
  idle: 'bg-gray-400', // Grey
  starting: 'bg-green-300', // Light green
  started: 'bg-green-500', // Green
  
  disconnected: 'bg-gray-400', // Grey
  connecting: 'bg-green-300', // Light green
  connected: 'bg-green-500', // Green

  failed: 'bg-red-500', // Red
};

function ActiveLobby() {
  const { activeLobby, setActiveLobby, usernameMap, loadingUsernames } = UseLobby();
  const [copied, setCopied] = useState(false);
  const [showSettings, setShowSettings] = useState(false);
  const [lobbyName, setLobbyName] = useState(activeLobby?.name || '');
  const [maxPlayers, setMaxPlayers] = useState(activeLobby?.max_players || 4);
  const [error, setError] = useState('');
  const [actionInProgress, setActionInProgress] = useState(false);
  
  const userData = JSON.parse(localStorage.getItem('user') || '{}');
  const isHost = activeLobby?.host === userData.user_id;

  useEffect(() => {
    // Update local state when the global active lobby changes
    if (activeLobby) {
      setLobbyName(activeLobby.name);
      setMaxPlayers(activeLobby.max_players);
    }
  }, [activeLobby]);

  const handleCopyId = () => {
    if (activeLobby) {
      navigator.clipboard.writeText(activeLobby.id);
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

    if (!activeLobby) return;

    try {
      // Call the API to update the lobby
      const updatedLobby = await updateLobby(activeLobby.id, {
        name: lobbyName,
        max_players: maxPlayers
      });
      
      // Update will come through socket, no need to set here
      setShowSettings(false);
    } catch (err) {
      setError(err.response?.data?.error || 'Failed to update lobby');
    }
  };
  
  const handleStartLobby = async () => {
    if (!activeLobby || actionInProgress) return;
    
    setActionInProgress(true);
    try {
      await startLobby(activeLobby.id);
      // The actual lobby update will come through the socket
    } catch (err) {
      setError(err.response?.data?.error || 'Failed to start lobby');
    } finally {
      // Keep button disabled for a short period to prevent spam
      setTimeout(() => setActionInProgress(false), 2000);
    }
  };
  
  const handleStopLobby = async () => {
    if (!activeLobby || actionInProgress) return;
    
    setActionInProgress(true);
    try {
      await stopLobby(activeLobby.id);
      // The actual lobby update will come through the socket
    } catch (err) {
      setError(err.response?.data?.error || 'Failed to stop lobby');
    } finally {
      // Keep button disabled for a short period to prevent spam
      setTimeout(() => setActionInProgress(false), 2000);
    }
  };

  // If no lobby data is available, show a loading state or return null
  if (!activeLobby) {
    return <div className="text-center py-10">Loading lobby...</div>;
  }

  // Get username display function
  const getUserDisplay = (userId) => {
    // If it's the current user, show "You"
    if (userId === userData.user_id) {
      return <span className="font-medium">You</span>;
    }
    
    // If we have the username in our map, show it
    if (usernameMap[userId]?.username) {
      return usernameMap[userId].username;
    }
    
    // Fallback to user ID (truncated for better UX)
    return `User ${userId.substring(0, 8)}...`;
  };
  
  // Get status indicator component
  const StatusIndicator = ({ status, className = "" }) => (
    <span 
      className={`inline-block w-2.5 h-2.5 rounded-full ${STATUS_COLORS[status] || 'bg-gray-400'} ${className}`}
      title={status.charAt(0).toUpperCase() + status.slice(1)}
    ></span>
  );

  return (
    <div className="bg-card border border-border rounded-lg overflow-hidden">
      <div className="p-6">
        <div className="flex justify-between items-center mb-6">
          <h2 className="text-xl font-semibold">{activeLobby.name}</h2>
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
          <div className="mb-8 p-4 bg-background rounded-lg shadow-md">
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
              <span className="font-mono">{activeLobby.id}</span>
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
            Players ({activeLobby.members.length}/{activeLobby.max_players})
          </h3>
          <div className="space-y-2">
            {loadingUsernames && activeLobby.members.length > 0 && (
              <div className="text-sm text-muted-foreground">Loading player information...</div>
            )}
            {activeLobby.members.map((memberId) => (
              <div 
                key={memberId}
                className="flex items-center justify-between p-2 rounded-lg hover:bg-accent/50"
              >
                <div className="flex items-center">
                  <StatusIndicator
                    status={activeLobby.members_status?.[memberId] || "disconnected"} 
                    className="mr-2"
                  />
                  <div className="w-8 h-8 rounded-full bg-primary/10 text-primary flex items-center justify-center mr-3">
                    {usernameMap[memberId]?.username 
                      ? usernameMap[memberId].username.charAt(0).toUpperCase() 
                      : memberId.slice(0, 1).toUpperCase()}
                  </div>
                  <div className="text-sm truncate">
                    {getUserDisplay(memberId)}
                    {memberId === activeLobby.host && (
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
      
      <div className="bg-secondary/40 p-4 flex justify-between items-center border-t border-border">
        <div className="text-sm flex items-center">
          <StatusIndicator status={activeLobby.status || "idle"} className="mr-2" />
          <span className="capitalize">{activeLobby.status || "idle"}</span>
        </div>
        {isHost && (
          activeLobby.status === "idle" ? (
            <button 
              className="px-4 py-2 bg-primary text-primary-foreground rounded-md text-sm flex items-center gap-1.5"
              disabled={activeLobby.members.length < 2 || actionInProgress}
              onClick={handleStartLobby}
            >
              <Play size={16} />
              Start Lobby
            </button>
          ) : (
            <button 
              className="px-4 py-2 bg-destructive text-destructive-foreground rounded-md text-sm flex items-center gap-1.5"
              disabled={actionInProgress}
              onClick={handleStopLobby}
            >
              <Square size={16} />
              Stop Lobby
            </button>
          )
        )}
      </div>
    </div>
  );
}

export default ActiveLobby; 