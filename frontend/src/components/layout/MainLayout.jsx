import React from 'react';
import { Outlet, useNavigate, useLocation } from 'react-router-dom';
import { useTheme } from '../../contexts/ThemeContext';
import { LobbyProvider } from '../../contexts/LobbyContext';
import LobbyIndicator from '../lobby/LobbyIndicator';
import { Home, Users, Settings as SettingsIcon, Info, LogOut, Moon, Sun } from 'lucide-react';
import { stopSocket } from '../../lib/socket';
const SidebarIcon = ({ icon, tooltip, active, onClick }) => {
  return (
    <div className={`sidebar-icon ${active ? 'bg-primary text-primary-foreground' : ''}`} onClick={onClick}>
      {icon}
      <span className="sidebar-tooltip group-hover:scale-100">{tooltip}</span>
    </div>
  );
};

function MainLayout() {
  const navigate = useNavigate();
  const location = useLocation();
  const { theme, toggleTheme } = useTheme();
  
  // Handle exit/logout
  const handleExit = () => {
    // Clear auth data
    localStorage.removeItem('user');
    stopSocket();
    // Navigate to landing
    navigate('/');
  };
  
  return (
    <LobbyProvider>
      <div className="flex h-screen">
        <div className="flex flex-col items-center bg-background dark:bg-card w-16 border-r border-border">
          <div className="flex flex-col items-center w-full flex-grow py-4">
            {/* Logo or app icon at the top */}
            <div className="mt-2 mb-6">
              <img src="/src/assets/logo.svg" alt="Logo" className="w-12 h-12" />
            </div>
            
            {/* Navigation Icons */}
            <div className="flex flex-col items-center space-y-2 flex-grow">
              <SidebarIcon 
                icon={<Home size={24} />} 
                tooltip="Lobby"
                active={location.pathname === '/app/lobby'}
                onClick={() => navigate('/app/lobby')}
              />
              <SidebarIcon 
                icon={<Users size={24} />} 
                tooltip="Friends"
                active={location.pathname === '/app/friends'}
                onClick={() => navigate('/app/friends')}
              />
              <SidebarIcon 
                icon={<SettingsIcon size={24} />} 
                tooltip="Settings"
                active={location.pathname === '/app/settings'}
                onClick={() => navigate('/app/settings')}
              />
              <SidebarIcon 
                icon={<Info size={24} />} 
                tooltip="About"
                active={location.pathname === '/app/about'}
                onClick={() => navigate('/app/about')}
              />
            </div>
            
            {/* Bottom icons */}
            <div className="mt-auto flex flex-col items-center space-y-2">
              <SidebarIcon 
                icon={theme === 'dark' ? <Sun size={24} /> : <Moon size={24} />} 
                tooltip={theme === 'dark' ? 'Light Mode' : 'Dark Mode'}
                onClick={toggleTheme}
              />
              <SidebarIcon 
                icon={<LogOut size={24} />} 
                tooltip="Exit"
                onClick={handleExit}
              />
            </div>
          </div>
        </div>

        <div className="flex-1 overflow-auto">
          <Outlet />
        </div>

        {location.pathname !== '/app/lobby' && <LobbyIndicator />}
      </div>
    </LobbyProvider>
  );
}

export default MainLayout;