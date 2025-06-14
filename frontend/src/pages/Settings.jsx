import React, { useState, useEffect } from 'react';

function Settings() {
  const [stunInfo, setStunInfo] = useState({
    publicIp: '',
    publicPort: '',
    loading: true,
    error: null,
  });

  useEffect(() => {
    const fetchStunInfo = async () => {
      try {
        const result = await window.electron.grpc.getStunInfo();
        if (result.error) {
          setStunInfo({
            publicIp: '',
            publicPort: '',
            loading: false,
            error: result.error
          });
        } else {
          setStunInfo({
            publicIp: result.publicIp,
            publicPort: result.publicPort,
            loading: false,
            error: result.errorMessage || null
          });
        }
      } catch (error) {
        setStunInfo({
          publicIp: '',
          publicPort: '',
          loading: false,
          error: error.message || 'Failed to get STUN info'
        });
      }
    };

    fetchStunInfo();
  }, []);

  return (
    <div className="h-full flex flex-col p-6">
      <h1 className="text-2xl font-bold mb-6">Settings</h1>
      
      <div className="shadow-md rounded-lg p-6 mb-6 bg-card border border-border">
        <h2 className="text-xl font-semibold mb-4">Network Information</h2>
        
        {stunInfo.loading ? (
          <p className="text-gray-600">Loading network information...</p>
        ) : stunInfo.error ? (
          <div className="text-red-500">
            <p>Error retrieving network info: {stunInfo.error}</p>
          </div>
        ) : (
          <div className="space-y-4">
            <div>
              <p className="text-gray-500 mb-1">Public IP Address</p>
              <p className="font-medium">{stunInfo.publicIp || 'Not available'}</p>
            </div>
            <div>
              <p className="text-gray-500 mb-1">Public Port</p>
              <p className="font-medium">{stunInfo.publicPort || 'Not available'}</p>
            </div>
          </div>
        )}
      </div>
    </div>
  );
}

export default Settings;