from flask import request, current_app, session
from flask_socketio import emit, join_room, leave_room
from .extensions import socketio
from .schemas import OnlineUser, Lobby
from flask_jwt_extended import decode_token

# TODO: Refactorize auth and reconnect

# Basic socket events

# Connect - receives auth token
@socketio.on('connect')
def handle_connect(auth=None):
    current_app.logger.debug(f'Client connected: {request.sid}')
    emit('connected', {'message': 'You are connected!'})

    user_id = session.get('user_id')
    if user_id:
        # We know this is a user that has reconnected
        online_user = OnlineUser.from_redis(user_id)
        if online_user:
            online_user.save_to_redis(expire_seconds=None)
            current_app.logger.debug(f'Set TTL to infinite for user {user_id}')
    
    # Emit current online count to the newly connected client
    online_users = OnlineUser.get_all_online_users()
    emit('online_count', {'count': len(online_users)})

@socketio.on('disconnect')
def handle_disconnect():
    current_app.logger.debug(f'Client disconnected: {request.sid}')
    # If user was authenticated, update their Redis TTL
    user_id = session.get('user_id')
    if user_id:
        # Set TTL to 60 seconds when disconnecting
        online_user = OnlineUser.from_redis(user_id)
        if online_user:
            online_user.save_to_redis(expire_seconds=60)
            current_app.logger.debug(f'Set TTL to 60s for user {user_id}')
        
        # Check if user was in any lobbies and remove them
        _handle_leave_all_lobbies(user_id)

@socketio.on('authenticate')
def handle_authenticate(data):
    token = data.get('token')
    if not token:
        emit('auth_error', {'message': 'No token provided'})
        return
    
    try:
        # Decode the JWT token
        decoded_token = decode_token(token)
        user_id = decoded_token.get('sub')  # 'sub' is where user_id is stored by default
        
        # Store user_id to session ID mapping
        session['user_id'] = user_id
        
        # Set Redis key to never expire (TTL = -1)
        online_user = OnlineUser.from_redis(user_id)
        if online_user:
            # Get current STUN info before making changes
            current_app.logger.debug(f'User {user_id} authenticated with STUN info: {online_user.to_dict()}')
            
            # Persist the user data without expiration
            online_user.save_to_redis(expire_seconds=None)
            current_app.logger.debug(f'Set TTL to infinite for user {user_id}')
            emit('authenticated', {'message': 'Authentication successful'})
            
            # Broadcast updated online count to all clients
            _broadcast_online_count()
        else:
            emit('auth_error', {'message': 'User not found in online registry'})
    except Exception as e:
        current_app.logger.error(f'Authentication error: {str(e)}')
        emit('auth_error', {'message': 'Invalid token'})

# Socket room management
@socketio.on('join_room')
def on_join_room(data):
    room = data.get('room')
    if not room:
        emit('error', {'message': 'Room name is required'})
        return
    
    join_room(room)
    emit('room_joined', {'room': room}, room=room)

@socketio.on('leave_room')
def on_leave_room(data):
    room = data.get('room')
    if not room:
        emit('error', {'message': 'Room name is required'})
        return
    
    leave_room(room)
    emit('room_left', {'room': room})

@socketio.on('message')
def handle_message(data):
    room = data.get('room')
    msg = data.get('message')
    emit('message', {'sid': request.sid, 'message': msg}, room=room)

# Helper functions
def _broadcast_online_count():
    """Broadcast online player count to all clients"""
    online_users = OnlineUser.get_all_online_users()
    socketio.emit('online_count', {'count': len(online_users)})

def _handle_leave_all_lobbies(user_id):
    """Remove user from all lobbies they're part of when they disconnect"""
    # Get all lobbies
    lobbies = Lobby.get_all_lobbies()
    
    for lobby_data in lobbies:
        if user_id in lobby_data.get('members', []):
            lobby_id = lobby_data.get('id')
            lobby = Lobby.from_redis(lobby_id)
            if lobby:
                lobby.remove_member(user_id)
                
                # If there are no members left, delete the lobby
                if not lobby.members:
                    current_app.redis_client.delete(f"lobby:{lobby_id}")
                    socketio.emit('lobby_deleted', {"lobby_id": lobby_id}, room=f"lobby_{lobby_id}")
                else:
                    # Otherwise, save the updated lobby
                    lobby.save_to_redis()
                    # Notify all remaining members about the update
                    socketio.emit('lobby_updated', lobby.to_dict(), room=f"lobby_{lobby_id}")
    
    # Broadcast updated online count
    _broadcast_online_count()

# Lobby socket handlers - just for real-time updates and room management
@socketio.on('join_lobby_room')
def handle_join_lobby_room(data):
    """Join the socket room for a lobby"""
    user_id = session.get('user_id')
    if not user_id:
        emit('lobby_error', {'message': 'Authentication required'})
        return
    
    lobby_id = data.get('lobby_id')
    if not lobby_id:
        emit('lobby_error', {'message': 'Lobby ID required'})
        return
    
    # Join the socket room for this lobby
    lobby_room = f"lobby_{lobby_id}"
    join_room(lobby_room)
    emit('joined_lobby_room', {'lobby_id': lobby_id})

@socketio.on('leave_lobby_room')
def handle_leave_lobby_room(data):
    """Leave the socket room for a lobby"""
    user_id = session.get('user_id')
    if not user_id:
        emit('lobby_error', {'message': 'Authentication required'})
        return
    
    lobby_id = data.get('lobby_id')
    if not lobby_id:
        emit('lobby_error', {'message': 'Lobby ID required'})
        return
    
    # Leave the socket room
    lobby_room = f"lobby_{lobby_id}"
    leave_room(lobby_room)
    emit('left_lobby_room', {'lobby_id': lobby_id})

@socketio.on('get_lobby_data')
def handle_get_lobby_data(data):
    """Get current lobby data"""
    user_id = session.get('user_id')
    if not user_id:
        emit('lobby_error', {'message': 'Authentication required'})
        return
    
    lobby_id = data.get('lobby_id')
    if not lobby_id:
        emit('lobby_error', {'message': 'Lobby ID required'})
        return
    
    # Get the lobby from Redis
    lobby = Lobby.from_redis(lobby_id)
    if not lobby:
        emit('lobby_error', {'message': 'Lobby not found'})
        return
    
    # Check if user is a member of the lobby
    if not lobby.is_member(user_id):
        emit('lobby_error', {'message': 'You are not a member of this lobby'})
        return
    
    emit('lobby_data', lobby.to_dict())
