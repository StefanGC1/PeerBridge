import React from 'react';
import logo from '../assets/logo.png';

const LoadingScreen = ({ message = "Loading..." }) => {
    // Check if looks better without logo
  return (
    <div className="fixed inset-0 z-50 flex flex-col items-center justify-center bg-black/20 backdrop-blur-sm">
      <div className="flex flex-col items-center space-y-6">
        <img src={logo} alt="PeerBridge Logo" className="w-20 h-20 animate-pulse" />
        <h2 className="text-2xl font-bold text-foreground">PeerBridge</h2>
        
        <div className="relative">
          <div className="w-12 h-12 border-4 border-primary/20 border-t-primary rounded-full animate-spin"></div>
        </div>
        
        <p className="text-base text-foreground font-medium">{message}</p>
      </div>
    </div>
  );
};

export default LoadingScreen; 