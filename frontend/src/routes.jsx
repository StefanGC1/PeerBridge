import React from 'react';
import { Navigate, useRoutes } from 'react-router-dom';

// Auth pages
import Landing from './pages/auth/Landing';
import Login from './pages/auth/Login';
import Register from './pages/auth/Register';

// Main app pages
import Lobby from './pages/Lobby';
import Friends from './pages/Friends';
import Settings from './pages/Settings';
import About from './pages/About';

// Layout
import MainLayout from './components/layout/MainLayout';

export default function Router() {
  // Simple authentication check - replace with your actual auth logic
  const isAuthenticated = () => {
    return localStorage.getItem('user') !== null;
  };

  return useRoutes([
    {
      path: '/',
      element: <Landing />,
    },
    {
      path: '/auth',
      children: [
        { path: 'login', element: <Login /> },
        { path: 'register', element: <Register /> },
      ],
    },
    {
      path: '/app',
      element: isAuthenticated() ? <MainLayout /> : <Navigate to="/auth/login" />,
      children: [
        { path: '', element: <Navigate to="/app/lobby" /> },
        { path: 'lobby', element: <Lobby /> },
        { path: 'friends', element: <Friends /> },
        { path: 'settings', element: <Settings /> },
        { path: 'about', element: <About /> },
      ],
    },
    {
      path: '*',
      element: <Navigate to="/" />,
    },
  ]);
}