import React from 'react';
import { Users, ArrowRightFromLine } from 'lucide-react';
import { useNavigate } from 'react-router-dom';
import { UseLobby } from '../../contexts/LobbyContext';
import { leaveLobby } from '../../lib/api';
import { leaveLobbyRoom } from '../../lib/socket';

function LobbyIndicator() {
  const { activeLobby, setActiveLobby } = UseLobby();
  const navigate = useNavigate();

  if (!activeLobby) return null;

  const handleGoToLobby = () => {
    navigate('/app/lobby');
  };

  const handleLeaveLobby = async (e) => {
    e.stopPropagation();
    try {
      // Call the API to leave the lobby
      leaveLobbyRoom(activeLobby.id);
      await leaveLobby(activeLobby.id);
      setActiveLobby(null);
    } catch (err) {
      console.error('Error leaving lobby:', err);
    }
  };

  return (
    <div 
      onClick={handleGoToLobby}
      className="fixed bottom-4 right-4 bg-card border border-border rounded-lg shadow-lg p-3 flex items-center gap-3 cursor-pointer hover:border-primary transition-colors"
    >
      <div className="w-10 h-10 rounded-full bg-primary/10 flex items-center justify-center text-primary">
        <Users size={20} />
      </div>
      <div>
        <div className="text-sm font-medium">{activeLobby.name}</div>
        <div className="text-xs text-muted-foreground flex items-center gap-1">
          <span>{activeLobby.members.length}/{activeLobby.max_players} players</span>
          <span className="inline-block w-1.5 h-1.5 rounded-full bg-green-500 ml-1"></span>
        </div>
      </div>
      <button 
        onClick={handleLeaveLobby}
        className="ml-2 p-1.5 rounded-full hover:bg-destructive/10 hover:text-destructive transition-colors"
        title="Leave Lobby"
      >
        <ArrowRightFromLine size={16} />
      </button>
    </div>
  );
}

export default LobbyIndicator; 