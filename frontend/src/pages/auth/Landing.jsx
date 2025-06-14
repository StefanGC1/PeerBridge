import React from 'react';
import { useNavigate } from 'react-router-dom';
import { useTheme } from '../../contexts/ThemeContext';
import { Moon, Sun } from 'lucide-react';
import logo from '../../assets/logo.png';

function Landing() {
  const navigate = useNavigate();
  const { theme, toggleTheme } = useTheme();
  
  return (
    <div className="flex flex-col min-h-screen bg-gradient-to-br from-background to-primary/10">
      <header className="flex justify-between items-center p-6">
        <div className="flex items-center gap-2">
          <img src={logo} alt="Logo"className="w-18 h-18"/>
          <h1 className="text-2xl font-bold text-foreground">PeerBridge</h1>
        </div>
        
        <button 
          onClick={toggleTheme} 
          className="p-2 rounded-full bg-background hover:bg-primary/10 transition-colors"
        >
          {theme === 'dark' ? <Sun size={20} /> : <Moon size={20} />}
        </button>
      </header>
      
      <main className="flex flex-1 flex-col items-center justify-center p-6">
        <div className="max-w-md w-full space-y-8 bg-card p-8 rounded-xl shadow-lg">
          <div className="text-center">
            <h2 className="text-3xl font-extrabold text-foreground mb-2">
              Welcome to PeerBridge
            </h2>
            <p className="text-muted-foreground">
              Connect and share with friends through a peer-to-peer network
            </p>
          </div>
          
          <div className="flex flex-col gap-4">
            <button
              onClick={() => navigate('/auth/login')}
              className="btn-primary w-full font-medium"
            >
              Sign In
            </button>
            
            <button
              onClick={() => navigate('/auth/register')}
              className="btn-secondary w-full font-medium"
            >
              Create Account
            </button>
          </div>
        </div>
      </main>
      
      <footer className="py-6 px-8 h-30 text-center text-sm text-muted-foreground">
      </footer>
    </div>
  );
}

export default Landing;