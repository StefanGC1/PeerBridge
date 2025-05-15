import React, { useState, useEffect } from 'react';
import { UserPlus, Search, User, UserX, RefreshCw, Circle } from 'lucide-react';
import { getFriends, searchUsers, addFriend, removeFriend } from '../lib/api';

function Friends() {
  const [friends, setFriends] = useState([]);
  const [isLoading, setIsLoading] = useState(true);
  const [error, setError] = useState(null);
  const [searchQuery, setSearchQuery] = useState('');
  const [searchResults, setSearchResults] = useState([]);
  const [isSearching, setIsSearching] = useState(false);
  const [addingFriend, setAddingFriend] = useState(false);

  useEffect(() => {
    fetchFriends();
  }, []);

  const fetchFriends = async () => {
    setIsLoading(true);
    setError(null);
    try {
      const friendsList = await getFriends();
      setFriends(friendsList);
    } catch (err) {
      setError('Failed to load friends. Please try again.');
      console.error('Error fetching friends:', err);
    } finally {
      setIsLoading(false);
    }
  };

  const handleSearch = async () => {
    if (searchQuery.length < 3) {
      setSearchResults([]);
      return;
    }

    setIsSearching(true);
    try {
      const results = await searchUsers(searchQuery);
      setSearchResults(results);
    } catch (err) {
      console.error('Error searching users:', err);
    } finally {
      setIsSearching(false);
    }
  };

  const handleAddFriend = async (userId) => {
    setAddingFriend(userId);
    try {
      await addFriend(userId);
      // Remove from search results
      setSearchResults(searchResults.filter(user => user.id !== userId));
      // Refresh friends list
      fetchFriends();
    } catch (err) {
      console.error('Error adding friend:', err);
    } finally {
      setAddingFriend(false);
    }
  };

  const handleRemoveFriend = async (userId) => {
    try {
      await removeFriend(userId);
      // Remove from friends list
      setFriends(friends.filter(friend => friend.id !== userId));
    } catch (err) {
      console.error('Error removing friend:', err);
    }
  };

  return (
    <div className="h-full flex flex-col">
      <div className="bg-card border-b border-border p-6">
        <div className="flex justify-between items-center">
          <h1 className="text-2xl font-bold text-foreground">Friends</h1>
          <div className="flex gap-2">
            <button
              onClick={fetchFriends}
              className="p-2 rounded-full hover:bg-primary/10 transition-colors text-foreground"
              aria-label="Refresh"
              disabled={isLoading}
            >
              <RefreshCw size={20} className={isLoading ? "animate-spin" : ""} />
            </button>
          </div>
        </div>
      </div>
      
      <div className="flex-1 overflow-auto p-6">
        <div className="mb-6">
          <h2 className="text-lg font-semibold mb-2">Find Friends</h2>
          <div className="flex gap-2">
            <div className="relative flex-grow">
              <input
                type="text"
                placeholder="Search by username..."
                className="input-field w-full pr-10"
                value={searchQuery}
                onChange={(e) => setSearchQuery(e.target.value)}
                onKeyPress={(e) => e.key === 'Enter' && handleSearch()}
              />
              {isSearching && (
                <div className="absolute right-3 top-1/2 transform -translate-y-1/2">
                  <RefreshCw size={16} className="animate-spin text-muted-foreground" />
                </div>
              )}
            </div>
            <button
              className="btn-primary flex justify-center items-center gap-1.5"
              onClick={handleSearch}
              disabled={searchQuery.length < 3 || isSearching}
            >
              <Search size={18} className="inline-block"/>
              <span>Search</span>
            </button>
          </div>
          
          {searchResults.length > 0 && (
            <div className="mt-4 bg-card border border-border rounded-md shadow-sm">
              <ul className="divide-y divide-border">
                {searchResults.map(user => (
                  <li key={user.id} className="p-3 flex items-center justify-between">
                    <div className="flex items-center">
                      <div className="w-8 h-8 rounded-full bg-primary/10 flex items-center justify-center text-primary">
                        <User size={16} />
                      </div>
                      <span className="ml-3 text-foreground">{user.username}</span>
                    </div>
                    <button
                      className="btn-primary btn-sm flex items-center gap-1"
                      onClick={() => handleAddFriend(user.id)}
                      disabled={addingFriend === user.id}
                    >
                      {addingFriend === user.id ? (
                        <RefreshCw size={14} className="animate-spin" />
                      ) : (
                        <UserPlus size={14} />
                      )}
                      <span>Add Friend</span>
                    </button>
                  </li>
                ))}
              </ul>
            </div>
          )}
        </div>
        
        <div>
          <h2 className="text-lg font-semibold mb-2">My Friends</h2>
          {isLoading ? (
            <div className="flex justify-center p-8">
              <RefreshCw size={24} className="animate-spin text-primary" />
            </div>
          ) : error ? (
            <div className="text-center p-8 text-red-500">
              <p>{error}</p>
              <button
                className="mt-2 btn-primary"
                onClick={fetchFriends}
              >
                Try Again
              </button>
            </div>
          ) : friends.length === 0 ? (
            <div className="text-center p-8 text-muted-foreground border border-dashed border-border rounded-md">
              <User size={48} className="mx-auto mb-2 opacity-30" />
              <p>You don't have any friends yet.</p>
              <p className="text-sm">Use the search above to find friends.</p>
            </div>
          ) : (
            <div className="bg-card border border-border rounded-md shadow-sm">
              <ul className="divide-y divide-border">
                {friends.map(friend => (
                  <li key={friend.id} className="p-4 flex items-center justify-between">
                    <div className="flex items-center">
                      <div className="relative">
                        <div className="w-10 h-10 rounded-full bg-primary/10 flex items-center justify-center text-primary">
                          <User size={20} />
                        </div>
                        {friend.online && (
                          <div className="absolute -bottom-1 -right-1 w-4 h-4 rounded-full bg-green-500 border-2 border-card" 
                               title="Online">
                          </div>
                        )}
                      </div>
                      <div className="ml-3">
                        <div className="text-foreground font-medium">{friend.username}</div>
                        <div className="text-xs text-muted-foreground flex items-center">
                          <Circle size={8} className={friend.online ? "text-green-500" : "text-gray-400"} />
                          <span className="ml-1">{friend.online ? 'Online' : 'Offline'}</span>
                        </div>
                      </div>
                    </div>
                    <div className="flex items-center gap-2">
                      <button
                        className="p-2 text-muted-foreground hover:text-red-500 hover:bg-red-500/10 rounded transition-colors"
                        onClick={() => handleRemoveFriend(friend.id)}
                        title="Remove friend"
                      >
                        <UserX size={18} />
                      </button>
                      {friend.online && (
                        <button
                          className="btn-primary btn-sm"
                        >
                          <span>Message</span>
                        </button>
                      )}
                    </div>
                  </li>
                ))}
              </ul>
            </div>
          )}
        </div>
      </div>
    </div>
  );
}

export default Friends;