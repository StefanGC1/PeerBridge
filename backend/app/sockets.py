from flask import request, current_app, session
from flask_socketio import emit, join_room, leave_room
from .extensions import socketio
from flask_jwt_extended import decode_token
import json
import uuid

# Some placeholder SocketIO events
# def register_socketio_events(app):
@socketio.on('connect')
def handle_connect():
    current_app.logger.debug(f'Client connected: {request.sid}')
    emit('connected', {'message': 'You are connected!'})
    
    # Emit current online count to the newly connected client
    redis_inst = current_app.redis_client
    online_count = len(redis_inst.keys("online:*"))
    emit('online_count', {'count': online_count})

@socketio.on('disconnect')
def handle_disconnect(sid=None):
    current_app.logger.debug(f'Client disconnected: {request.sid}')
    # If user was authenticated, update their Redis TTL
    user_id = session.get('user_id')
    if user_id:
        # Set TTL to 60 seconds when disconnecting
        redis_inst = current_app.redis_client
        key = f"online:{user_id}"
        if redis_inst.exists(key):
            # Set expiration
            redis_inst.expire(key, 60)
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
        redis_inst = current_app.redis_client
        key = f"online:{user_id}"
        if redis_inst.exists(key):
            # Get current STUN info before making changes
            stun_info = redis_inst.hgetall(key)
            current_app.logger.debug(f'User {user_id} authenticated with STUN info: {stun_info}')
            
            # Preserve the STUN info but remove expiration
            redis_inst.persist(key)  # Remove expiration, set to infinite TTL
            current_app.logger.debug(f'Set TTL to infinite for user {user_id}')
            emit('authenticated', {'message': 'Authentication successful'})
            
            # Broadcast updated online count to all clients
            _broadcast_online_count()
        else:
            emit('auth_error', {'message': 'User not found in online registry'})
    except Exception as e:
        current_app.logger.error(f'Authentication error: {str(e)}')
        emit('auth_error', {'message': 'Invalid token'})

@socketio.on('join')
def on_join(data):
    room = data.get('room')
    join_room(room)
    emit('status', {'message': f'{request.sid} has entered the room.'}, room=room)

@socketio.on('leave')
def on_leave(data):
    room = data.get('room')
    leave_room(room)
    emit('status', {'message': f'{request.sid} has left the room.'}, room=room)

@socketio.on('message')
def handle_message(data):
    room = data.get('room')
    msg = data.get('message')
    emit('message', {'sid': request.sid, 'message': msg}, room=room)

# Helper function to broadcast online player count to all clients
def _broadcast_online_count():
    redis_inst = current_app.redis_client
    online_count = len(redis_inst.keys("online:*"))
    socketio.emit('online_count', {'count': online_count})

# Lobby management functions
def _handle_leave_all_lobbies(user_id):
    """Remove user from all lobbies they're part of when they disconnect"""
    redis_inst = current_app.redis_client
    # Get all lobbies
    lobby_keys = redis_inst.keys("lobby:*")
    
    for lobby_key in lobby_keys:
        lobby_data = json.loads(redis_inst.get(lobby_key))
        if user_id in lobby_data.get('members', []):
            lobby_id = lobby_key.split(':')[1]
            _remove_user_from_lobby(user_id, lobby_id)
    
    # Broadcast updated online count
    _broadcast_online_count()

def _remove_user_from_lobby(user_id, lobby_id):
    """Internal function to remove a user from a lobby"""
    redis_inst = current_app.redis_client
    lobby_key = f"lobby:{lobby_id}"
    
    if not redis_inst.exists(lobby_key):
        return False
    
    lobby_data = json.loads(redis_inst.get(lobby_key))
    members = lobby_data.get('members', [])
    
    if user_id in members:
        members.remove(user_id)
        
        # If host left, assign a new host or delete the lobby
        if lobby_data['host'] == user_id:
            if members:
                lobby_data['host'] = members[0]  # Assign first remaining member as host
            else:
                # Delete empty lobby
                redis_inst.delete(lobby_key)
                return True
        
        lobby_data['members'] = members
        redis_inst.set(lobby_key, json.dumps(lobby_data))
        
        # Notify all remaining members about the change
        socketio.emit('lobby_updated', lobby_data, room=f"lobby_{lobby_id}")
    
    return True

@socketio.on('create_lobby')
def handle_create_lobby(data):
    user_id = session.get('user_id')
    if not user_id:
        emit('lobby_error', {'message': 'Authentication required'})
        return
    
    lobby_name = data.get('name', 'Unnamed Lobby')
    max_players = data.get('max_players', 4)
    
    # Create a new lobby
    lobby_id = str(uuid.uuid4())
    lobby_data = {
        'id': lobby_id,
        'name': lobby_name,
        'host': user_id,
        'members': [user_id],
        'max_players': max_players,
        'status': 'open'
    }
    
    # Store in Redis with a 3-hour TTL (to prevent orphaned lobbies)
    redis_inst = current_app.redis_client
    redis_inst.set(f"lobby:{lobby_id}", json.dumps(lobby_data), ex=10800)  # 3 hours
    
    # Join the socket room for this lobby
    join_room(f"lobby_{lobby_id}")
    
    emit('lobby_created', lobby_data)
    current_app.logger.debug(f'User {user_id} created lobby {lobby_id}')

@socketio.on('join_lobby')
def handle_join_lobby(data):
    user_id = session.get('user_id')
    if not user_id:
        emit('lobby_error', {'message': 'Authentication required'})
        return
    
    lobby_id = data.get('lobby_id')
    if not lobby_id:
        emit('lobby_error', {'message': 'Lobby ID required'})
        return
    
    redis_inst = current_app.redis_client
    lobby_key = f"lobby:{lobby_id}"
    
    if not redis_inst.exists(lobby_key):
        emit('lobby_error', {'message': 'Lobby not found'})
        return
    
    lobby_data = json.loads(redis_inst.get(lobby_key))
    
    # Check if lobby is full
    if len(lobby_data['members']) >= lobby_data['max_players']:
        emit('lobby_error', {'message': 'Lobby is full'})
        return
    
    # Check if user is already in the lobby
    if user_id in lobby_data['members']:
        emit('lobby_error', {'message': 'Already in this lobby'})
        return
    
    # Add user to lobby
    lobby_data['members'].append(user_id)
    redis_inst.set(lobby_key, json.dumps(lobby_data))
    
    # Join the socket room for this lobby
    join_room(f"lobby_{lobby_id}")
    
    # Notify all members about the new user
    socketio.emit('lobby_updated', lobby_data, room=f"lobby_{lobby_id}")
    
    emit('lobby_joined', lobby_data)
    current_app.logger.debug(f'User {user_id} joined lobby {lobby_id}')

@socketio.on('leave_lobby')
def handle_leave_lobby(data):
    user_id = session.get('user_id')
    if not user_id:
        emit('lobby_error', {'message': 'Authentication required'})
        return
    
    lobby_id = data.get('lobby_id')
    if not lobby_id:
        emit('lobby_error', {'message': 'Lobby ID required'})
        return
    
    success = _remove_user_from_lobby(user_id, lobby_id)
    
    if success:
        # Leave the socket room
        leave_room(f"lobby_{lobby_id}")
        emit('lobby_left', {'lobby_id': lobby_id})
        current_app.logger.debug(f'User {user_id} left lobby {lobby_id}')
    else:
        emit('lobby_error', {'message': 'Error leaving lobby'})

@socketio.on('list_lobbies')
def handle_list_lobbies():
    user_id = session.get('user_id')
    if not user_id:
        emit('lobby_error', {'message': 'Authentication required'})
        return
    
    redis_inst = current_app.redis_client
    lobby_keys = redis_inst.keys("lobby:*")
    lobbies = []
    
    for key in lobby_keys:
        lobby_data = json.loads(redis_inst.get(key))
        # Filter out private or closed lobbies if needed
        if lobby_data.get('status') == 'open':
            lobbies.append(lobby_data)
    
    emit('lobbies_list', {'lobbies': lobbies})

@socketio.on('update_lobby')
def handle_update_lobby(data):
    user_id = session.get('user_id')
    if not user_id:
        emit('lobby_error', {'message': 'Authentication required'})
        return
    
    lobby_id = data.get('lobby_id')
    if not lobby_id:
        emit('lobby_error', {'message': 'Lobby ID required'})
        return
    
    redis_inst = current_app.redis_client
    lobby_key = f"lobby:{lobby_id}"
    
    if not redis_inst.exists(lobby_key):
        emit('lobby_error', {'message': 'Lobby not found'})
        return
    
    lobby_data = json.loads(redis_inst.get(lobby_key))
    
    # Only the host can update the lobby
    if lobby_data['host'] != user_id:
        emit('lobby_error', {'message': 'Only the host can update the lobby'})
        return
    
    # Update allowed fields
    if 'name' in data:
        lobby_data['name'] = data['name']
    if 'max_players' in data:
        lobby_data['max_players'] = data['max_players']
    if 'status' in data:
        lobby_data['status'] = data['status']
    
    redis_inst.set(lobby_key, json.dumps(lobby_data))
    
    # Notify all members about the update
    socketio.emit('lobby_updated', lobby_data, room=f"lobby_{lobby_id}")
    emit('lobby_update_success', {'message': 'Lobby updated successfully'})
