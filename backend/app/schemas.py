from dataclasses import dataclass, asdict, field
from typing import List, Dict, Optional, Any
import json
import uuid
from datetime import datetime
from flask import current_app


@dataclass
class OnlineUser:
    user_id: str
    ip: str
    port: int
    last_active: str = field(default_factory=lambda: datetime.utcnow().isoformat())
    
    @classmethod
    def from_redis(cls, user_id: str, redis_instance=None):
        """Create an OnlineUser instance from Redis data"""
        redis_inst = redis_instance or current_app.redis_client
        key = f"online:{user_id}"
        
        if not redis_inst.exists(key):
            return None
        
        data = redis_inst.hgetall(key)
        return cls(
            user_id=user_id,
            ip=data.get('ip', ''),
            port=int(data.get('port', 0)),
            last_active=data.get('last_active', datetime.utcnow().isoformat())
        )
    
    def save_to_redis(self, expire_seconds=None, redis_instance=None):
        """Save user data to Redis with optional expiration"""
        redis_inst = redis_instance or current_app.redis_client
        key = f"online:{self.user_id}"
        
        # Update last_active timestamp if not explicitly setting it
        if not hasattr(self, 'last_active') or not self.last_active:
            self.last_active = datetime.utcnow().isoformat()
        
        # Convert to dict and save to Redis
        data = {
            'ip': self.ip,
            'port': str(self.port),
            'last_active': self.last_active
        }
        
        redis_inst.hset(key, mapping=data)
        
        # Set expiration if specified
        if expire_seconds is not None:
            redis_inst.expire(key, expire_seconds)
        
        return True
    
    def to_dict(self):
        """Convert to dictionary"""
        return asdict(self)
    
    @classmethod
    def get_all_online_users(cls, redis_instance=None):
        """Get all online users from Redis"""
        redis_inst = redis_instance or current_app.redis_client
        keys = redis_inst.keys("online:*")
        
        online_users = {}
        for key in keys:
            user_id = key.split(":")[1]
            user = cls.from_redis(user_id, redis_inst)
            if user:
                online_users[user_id] = user.to_dict()
        
        return online_users


@dataclass
class Lobby:
    id: str = field(default_factory=lambda: str(uuid.uuid4()))
    name: str = "Unnamed Lobby"
    host: str = None
    members: List[str] = field(default_factory=list)
    max_players: int = 4
    status: str = "open"
    created_at: str = field(default_factory=lambda: datetime.utcnow().isoformat())
    
    def __post_init__(self):
        """Ensure host is in members list"""
        if self.host and self.host not in self.members:
            self.members.append(self.host)
    
    @classmethod
    def from_redis(cls, lobby_id: str, redis_instance=None):
        """Create a Lobby instance from Redis data"""
        redis_inst = redis_instance or current_app.redis_client
        key = f"lobby:{lobby_id}"
        
        if not redis_inst.exists(key):
            return None
        
        data = json.loads(redis_inst.get(key))
        return cls(**data)
    
    def save_to_redis(self, expire_seconds=10800, redis_instance=None):
        """Save lobby data to Redis with optional expiration (default 3 hours)"""
        redis_inst = redis_instance or current_app.redis_client
        key = f"lobby:{self.id}"
        
        # Convert to dict, then to JSON and save to Redis
        redis_inst.set(key, json.dumps(self.to_dict()), ex=expire_seconds)
        return True
    
    def to_dict(self):
        """Convert to dictionary"""
        return asdict(self)
    
    def add_member(self, user_id: str):
        """Add a member to the lobby"""
        if user_id in self.members:
            return False
        
        if len(self.members) >= self.max_players:
            return False
        
        self.members.append(user_id)
        return True
    
    def remove_member(self, user_id: str):
        """Remove a member from the lobby"""
        if user_id not in self.members:
            return False
        
        self.members.remove(user_id)
        
        # If host left, assign a new host or mark for deletion
        if self.host == user_id and self.members:
            self.host = self.members[0]
        
        return True
    
    def is_member(self, user_id: str):
        """Check if a user is a member of the lobby"""
        return user_id in self.members
    
    def is_host(self, user_id: str):
        """Check if a user is the host of the lobby"""
        return user_id == self.host
    
    def is_full(self):
        """Check if the lobby is full"""
        return len(self.members) >= self.max_players
    
    def update(self, name=None, max_players=None, status=None):
        """Update lobby settings"""
        if name is not None:
            self.name = name
        
        if max_players is not None:
            self.max_players = max_players
        
        if status is not None:
            self.status = status
        
        return True
    
    @classmethod
    def get_all_lobbies(cls, status_filter=None, redis_instance=None):
        """Get all lobbies from Redis with optional status filter"""
        redis_inst = redis_instance or current_app.redis_client
        keys = redis_inst.keys("lobby:*")
        
        lobbies = []
        for key in keys:
            lobby_id = key.split(":")[1]
            lobby = cls.from_redis(lobby_id, redis_inst)
            
            if lobby:
                # Apply status filter if provided
                if status_filter is None or lobby.status == status_filter:
                    lobbies.append(lobby.to_dict())
        
        return lobbies 