import base64
import math
from flask import copy_current_request_context, request, current_app, session
from flask_socketio import emit, join_room, leave_room
from .extensions import socketio
from .schemas import OnlineUser, Lobby
from flask_jwt_extended import decode_token

# Connect - receives auth token
@socketio.on('connect')
def handle_connect(auth=None):
    if not auth:
        current_app.logger.warning("Auth not present in event data")
        emit('auth_error', {'message': 'No auth data provided'})
        return False
    
    token = auth.get('token')
    if not token:
        emit('auth_error', {'message': 'No token provided'})
        return False
    
    try:
        # Decode the JWT token
        decoded_token = decode_token(token)
        user_id = decoded_token.get('sub')  # 'sub' is where user_id is stored by default
        
        # Store user_id to session ID mapping
        session['user_id'] = user_id
        
        # Set Redis key TTL to infinite
        online_user = OnlineUser.from_redis(user_id)

        # This handles reconnection automatically
        if online_user:
            # Get current STUN info before making changes
            # TODO later: Remove this log
            current_app.logger.debug(f'User {user_id} authenticated with STUN info: '
                                     f'IP: {online_user.ip}, '
                                     f'Port: {online_user.port}, '
                                     f'Public Key: {online_user.public_key[:5].hex() if online_user.public_key else "None"}') # TODO: Remove if later
            
            # Persist redis data
            online_user.save_to_redis(expire_seconds=math.inf)
            emit('connected', {'message': 'Authentication successful'})
            
            current_app.logger.debug(f'Set TTL to infinite for user {user_id}')
            current_app.logger.debug(f'Client connected: SID: {request.sid}')

            # Broadcast updated online count to all clients
            _broadcast_online_count()
        else:
            emit('auth_error', {'message': 'User not found in online registry'})
    except Exception as e:
        current_app.logger.error(f'Authentication error: {str(e)}')
        emit('auth_error', {'message': 'Invalid token'})
        return False

@socketio.on('disconnect')
def handle_disconnect(what=None):
    current_app.logger.debug(f'Client disconnected: {request.sid}')

    user_id = session.pop('user_id', None)
    if not user_id:
        current_app.logger.warning(f'User {user_id} not found in session')
        return
    
    online_user = OnlineUser.from_redis(user_id)
    if online_user:
        online_user.save_to_redis(expire_seconds=60)
        current_app.logger.debug(f'Set TTL to 60s for user {user_id}')
    else:
        current_app.logger.warning(f'User {user_id} not found in online registry')
        _handle_leave_lobby(user_id)

    # Check bookmarks for fix in case this breaks
    @copy_current_request_context
    def _leave_lobby_background_task(user_id):
        socketio.sleep(61)
        if OnlineUser.from_redis(user_id):
            return

        _handle_leave_lobby(user_id)
        current_app.logger.debug(f'User {user_id} left lobby after 60 seconds')
    
    socketio.start_background_task(_leave_lobby_background_task, user_id)

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

def _handle_leave_lobby(user_id):
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
                    socketio.emit('lobby_deleted',
                        {"lobby_id": lobby_id}, room=f"lobby_{lobby_id}")
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

@socketio.on('lobby_user_connected')
def handle_lobby_user_connected(data):
    """User successfully connected to peers"""
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
    
    # Update user status to connected
    lobby.set_member_status(user_id, "connected")
    
    # Check if at least 2 users are connected
    connected_count = sum(1 for status in lobby.members_status.values() if status == "connected")
    
    # If at least 2 users are connected, mark lobby as started
    if connected_count >= 2 and lobby.status == "starting":
        lobby.update(status="started")
    
    # Save the updated lobby
    lobby.save_to_redis()
    current_app.logger.debug(f'User {user_id} connected to peers, lobby status: {lobby.status}')
    
    # Notify all members about the update
    socketio.emit('lobby_updated', lobby.to_dict(), room=f"lobby_{lobby_id}")

@socketio.on('lobby_user_failure')
def handle_lobby_user_failure(data):
    """User failed to connect to peers"""
    user_id = session.get('user_id')
    if not user_id:
        emit('lobby_error', {'message': 'Authentication required'})
        return
    
    lobby_id = data.get('lobby_id')
    if not lobby_id:
        emit('lobby_error', {'message': 'Lobby ID required'})
        return
    
    error = data.get('error', 'Unknown error')
    
    # Get the lobby from Redis
    lobby = Lobby.from_redis(lobby_id)
    if not lobby:
        emit('lobby_error', {'message': 'Lobby not found'})
        return
    
    # Check if user is a member of the lobby
    if not lobby.is_member(user_id):
        emit('lobby_error', {'message': 'You are not a member of this lobby'})
        return
    
    # Update user status to failed
    lobby.set_member_status(user_id, "failed")
    current_app.logger.debug(f'User {user_id} failed to connect to peers, lobby status: {lobby.status}')
    
    # Check if all but one user has failed
    failed_count = sum(1 for status in lobby.members_status.values() if status == "failed")
    
    # If enough users have failed, mark lobby as failed and stop it
    if failed_count >= len(lobby.members) - 1 and lobby.status == "starting":
        lobby.update(status="failed")
        lobby.set_all_members_status("disconnected")
        current_app.logger.debug(f'Lobby {lobby_id} marked as failed and stopped')
        
        # Notify all members about the lobby stopping
        socketio.emit('lobby_stopping', lobby.to_dict(), room=f"lobby_{lobby_id}")
    else:
        # Just update the lobby
        lobby.save_to_redis()
        
        # Notify all members about the update
        socketio.emit('lobby_updated', lobby.to_dict(), room=f"lobby_{lobby_id}")
