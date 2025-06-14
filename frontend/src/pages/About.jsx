import React, { useState, useEffect } from 'react';

function About() {
  return (
    <div className="h-full flex flex-col p-6">
      <h1 className="text-2xl font-bold mb-6">About</h1>

      <div className="shadow-md rounded-lg p-6 bg-card border border-border">
        <p className="text-gray-500">PeerBridge v1.0.0</p>
        <p className="text-gray-500">Bachelor Thesis Project</p>
      </div>
      
    </div>
  );
}

export default About;