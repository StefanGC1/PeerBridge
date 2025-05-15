import React, { useState } from 'react';
import { X } from 'lucide-react';
import { joinLobby } from '../../lib/socket';

function JoinLobbyModal({ onClose, onLobbyJoined }) {
  const [lobbyId, setLobbyId] = useState('');
  const [isJoining, setIsJoining] = useState(false);
  const [error, setError] = useState('');

  const handleSubmit = async (e) => {
    e.preventDefault();
    
    if (!lobbyId.trim()) {
      setError('Please enter a lobby ID');
      return;
    }
    
    setError('');
    setIsJoining(true);

    try {
      const lobbyData = await joinLobby(lobbyId.trim());
      onLobbyJoined(lobbyData);
    } catch (err) {
      setError(err.message || 'Failed to join lobby. Check the ID and try again.');
    } finally {
      setIsJoining(false);
    }
  };

  return (
    <div className="fixed inset-0 bg-background/80 flex items-center justify-center z-50">
      <div className="bg-card border border-border rounded-lg shadow-lg w-full max-w-md p-6">
        <div className="flex justify-between items-center mb-4">
          <h2 className="text-xl font-semibold">Join a Lobby</h2>
          <button onClick={onClose} className="text-muted-foreground hover:text-foreground">
            <X size={20} />
          </button>
        </div>

        {error && (
          <div className="bg-destructive/10 text-destructive text-sm p-3 rounded mb-4">
            {error}
          </div>
        )}

        <form onSubmit={handleSubmit}>
          <div className="space-y-4">
            <div>
              <label htmlFor="lobbyId" className="block text-sm font-medium text-foreground mb-1">
                Lobby ID
              </label>
              <input
                type="text"
                id="lobbyId"
                value={lobbyId}
                onChange={(e) => setLobbyId(e.target.value)}
                placeholder="Enter the lobby ID"
                className="w-full px-3 py-2 bg-background border border-input rounded-md focus:outline-none focus:ring-2 focus:ring-primary"
              />
              <p className="text-xs text-muted-foreground mt-1">
                Enter the ID shared by the lobby creator
              </p>
            </div>

            <div className="flex justify-end space-x-2 pt-4">
              <button
                type="button"
                onClick={onClose}
                className="px-4 py-2 border border-border rounded-md text-foreground hover:bg-accent transition-colors"
              >
                Cancel
              </button>
              <button
                type="submit"
                disabled={isJoining}
                className="px-4 py-2 bg-primary text-primary-foreground rounded-md hover:bg-primary/90 disabled:opacity-50 disabled:cursor-not-allowed transition-colors"
              >
                {isJoining ? 'Joining...' : 'Join Lobby'}
              </button>
            </div>
          </div>
        </form>
      </div>
    </div>
  );
}

export default JoinLobbyModal; 