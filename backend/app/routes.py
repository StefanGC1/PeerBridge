from datetime import timedelta
from flask import Blueprint, request, jsonify, current_app
from .extensions import db, socketio
from .models import User
from .schemas import OnlineUser, Lobby
from flask_jwt_extended import (
    create_access_token,
    jwt_required,
    get_jwt_identity
)
from flask import copy_current_request_context

bp = Blueprint('api', __name__)

@bp.route('/register', methods=['POST'])
def register():
    data = request.get_json() or {}
    username = data.get('username')
    password = data.get('password')

    if not username or not password:
        return jsonify({'error': 'username and password required'}), 400

    if User.query.filter_by(username=username).first():
        return jsonify({'error': 'username already taken'}), 409

    user = User(username=username)
    user.set_password(password)
    db.session.add(user)
    db.session.commit()

    # Get STUN info from request
    try:
        user_ip = data.get('public_ip', '')
        user_port = int(data.get('public_port', 0))
        public_key = data.get('public_key', [])
    except (ValueError, TypeError) as e:
        current_app.logger.warning(f"Invalid STUN port format: {e}. Using default.")
        user_ip = data.get('public_ip', '')
        user_port = 0
        public_key = []

    user_id = user.id

    public_key = bytes(public_key)

    # Print STUN info to console
    current_app.logger.debug(f"User {username} (ID: {user_id}) registered with STUN info - "
                             f"IP: {user_ip}, Port: {user_port}, "
                             f"Public Key: {public_key[:5].hex() if public_key else 'None'}")

    # Store user in Redis using OnlineUser class
    online_user = OnlineUser(
        user_id=user_id,
        ip=user_ip,
        port=user_port,
        public_key=public_key
    )
    online_user.save_to_redis(expire_seconds=60)  # 60 seconds initial TTL

    access_token = create_access_token(identity=user.id, expires_delta=timedelta(hours=12))
    return jsonify({
        "message": f"User {user_id} registered",
        "access_token": access_token,
        "user_id": user_id,
        "username": username
    }), 201

@bp.route("/login", methods=["POST"])
def login():
    data = request.get_json() or {}
    username = data.get('username')
    password = data.get('password')

    if not username or not password:
        return jsonify({'error': 'username and password required'}), 400

    user = User.query.filter_by(username=username).first()
    if not user or not user.check_password(password):
        return jsonify({'error': 'invalid username or password'}), 401
    
    # Get STUN info from request
    try:
        user_ip = data.get('public_ip', '')
        user_port = int(data.get('public_port', 0))
        public_key = data.get('public_key', [])
    except (ValueError, TypeError) as e:
        current_app.logger.warning(f"Invalid STUN port format: {e}. Using default.")
        user_ip = data.get('public_ip', '')
        user_port = 0
        public_key = []

    user_id = user.id

    public_key = bytes(public_key)

    # Print STUN info to console
    current_app.logger.debug(f"User {username} (ID: {user_id}) logged in with STUN info - "
                             f"IP: {user_ip}, Port: {user_port}, "
                             f"Public Key: {public_key[:5].hex() if public_key else 'None'}")

    # Store user in Redis using OnlineUser class
    online_user = OnlineUser(
        user_id=user_id,
        ip=user_ip,
        port=user_port,
        public_key=public_key
    )
    online_user.save_to_redis(expire_seconds=60)  # 60 seconds initial TTL

    access_token = create_access_token(identity=user.id, expires_delta=timedelta(hours=12))
    return jsonify({
        "message": f"User {user_id} logged in",
        "access_token": access_token,
        "user_id": user_id,
        "username": username
    }), 200

@bp.route("/online", methods=["GET"])
@jwt_required()
def get_online_users():
    current_user_id = get_jwt_identity()
    
    # Get all online users using the OnlineUser class
    online_users = OnlineUser.get_all_online_users()
    
    return jsonify({
        "you": current_user_id,
        "online": online_users}), 200

# Friend management endpoints
@bp.route("/friends", methods=["GET"])
@jwt_required()
def get_friends():
    current_user_id = get_jwt_identity()
    
    # Get the current user
    current_user = User.query.get(current_user_id)
    if not current_user:
        return jsonify({"error": "User not found"}), 404
    
    # Get all friends of current user
    friends = current_user.get_friends()
    
    # Get online users from Redis
    online_users = OnlineUser.get_all_online_users()
    online_user_ids = online_users.keys()
    
    # Convert to dictionary and add online status
    friends_list = []
    for friend in friends:
        friend_data = friend.to_dict()
        friend_data['online'] = friend.id in online_user_ids
        if friend.id in online_user_ids:
            friend_data['connection_info'] = online_users[friend.id]
        friends_list.append(friend_data)
    
    return jsonify({
        "user_id": current_user_id,
        "friends": friends_list
    }), 200

@bp.route("/friends/add/<user_id>", methods=["POST"])
@jwt_required()
def add_friend(user_id):
    current_user_id = get_jwt_identity()
    
    # Check that the user is not trying to add themselves
    if current_user_id == user_id:
        return jsonify({"error": "Cannot add yourself as a friend"}), 400
    
    # Get the current user
    current_user = User.query.get(current_user_id)
    if not current_user:
        return jsonify({"error": "Current user not found"}), 404
    
    # Get the user to add as a friend
    friend = User.query.get(user_id)
    if not friend:
        return jsonify({"error": "Friend user not found"}), 404
    
    # Add the friend
    if current_user.add_friend(friend):
        db.session.commit()
        return jsonify({
            "message": f"User {friend.username} added as a friend",
            "friend": friend.to_dict()
        }), 201
    else:
        return jsonify({"message": "User is already a friend"}), 200

@bp.route("/friends/remove/<user_id>", methods=["DELETE"])
@jwt_required()
def remove_friend(user_id):
    current_user_id = get_jwt_identity()
    
    # Get the current user
    current_user = User.query.get(current_user_id)
    if not current_user:
        return jsonify({"error": "Current user not found"}), 404
    
    # Get the user to remove from friends
    friend = User.query.get(user_id)
    if not friend:
        return jsonify({"error": "Friend user not found"}), 404
    
    # Remove the friend
    if current_user.remove_friend(friend):
        db.session.commit()
        return jsonify({"message": f"User {friend.username} removed from friends"}), 200
    else:
        return jsonify({"error": "User is not in your friends list"}), 404

@bp.route("/users/search", methods=["GET"])
@jwt_required()
def search_users():
    current_user_id = get_jwt_identity()
    query = request.args.get('q', '')
    
    if not query or len(query) < 3:
        return jsonify({"error": "Search query must be at least 3 characters"}), 400
    
    # Search for users by username (case-insensitive)
    users = User.query.filter(User.username.ilike(f"%{query}%")).all()
    
    # Convert to list of dictionaries and exclude the current user
    users_list = [user.to_dict() for user in users if user.id != current_user_id]
    
    return jsonify({"users": users_list}), 200

# New lobby management endpoints
@bp.route("/lobbies/create-lobby", methods=["POST"])
@jwt_required()
def create_lobby():
    current_user_id = get_jwt_identity()
    data = request.get_json() or {}
    
    lobby_name = data.get('name', 'Unnamed Lobby')
    max_players = data.get('max_players', 4)
    
    # Create a new lobby using the Lobby class
    lobby = Lobby(
        name=lobby_name,
        host=current_user_id,
        members=[current_user_id],
        max_players=max_players,
        status='idle'
    )
    
    # Save the lobby to Redis
    lobby.save_to_redis()
    
    # Send a real-time update to the user
    # This might be redundant
    # lobby_room = f"lobby_{lobby.id}"
    # socketio.emit('lobby_created', lobby.to_dict(), room=request.sid)
    
    # Return the created lobby
    return jsonify(lobby.to_dict()), 201

@bp.route("/lobbies/join-lobby/<lobby_id>", methods=["POST"])
@jwt_required()
def join_lobby(lobby_id):
    current_user_id = get_jwt_identity()

    lobby = Lobby.from_redis(lobby_id)

    # Temporary quality of life fix to join lobby by name
    if not lobby:
        all_lobbies = Lobby.get_all_lobbies()
        target_lobby_data = next((l for l in all_lobbies if l.get("name") == lobby_id), None)
        if target_lobby_data:
            # Retrieve the lobby again by its real ID
            real_id = target_lobby_data["id"]
            lobby = Lobby.from_redis(real_id)
            lobby_id = real_id

    # Join checks
    if not lobby:
        return jsonify({"error": "Lobby not found"}), 404

    if lobby.is_full():
        return jsonify({"error": "Lobby is full"}), 400

    if lobby.status != 'idle':
        return jsonify({"error": "Lobby is not open for joining"}), 400

    if lobby.is_member(current_user_id):
        return jsonify({"error": "Already in this lobby"}), 400

    lobby.add_member(current_user_id)
    lobby.save_to_redis()

    socketio.emit('lobby_updated', lobby.to_dict(), room=f"lobby_{lobby_id}")
    
    return jsonify(lobby.to_dict()), 200

@bp.route("/lobbies/leave-lobby/<lobby_id>", methods=["POST"])
@jwt_required()
def leave_lobby(lobby_id):
    current_user_id = get_jwt_identity()
    
    # Get the lobby from Redis
    lobby = Lobby.from_redis(lobby_id)
    if not lobby:
        return jsonify({"error": "Lobby not found"}), 404
    
    # Check if user is in the lobby
    if not lobby.is_member(current_user_id):
        return jsonify({"error": "Not a member of this lobby"}), 400
    
    # Remove user from lobby
    lobby.remove_member(current_user_id)
    
    # If there are no members left, delete the lobby
    if not lobby.members:
        current_app.redis_client.delete(f"lobby:{lobby_id}")
        return jsonify({"message": "Lobby deleted"}), 200
    
    # Otherwise, save the updated lobby
    lobby.save_to_redis()
    
    # Notify all remaining members about the update
    socketio.emit('lobby_updated', lobby.to_dict(), room=f"lobby_{lobby_id}")
    
    return jsonify({"message": "Successfully left the lobby"}), 200

@bp.route("/lobbies/update-lobby/<lobby_id>", methods=["PUT"])
@jwt_required()
def update_lobby(lobby_id):
    current_user_id = get_jwt_identity()
    data = request.get_json() or {}
    
    # Get the lobby from Redis
    lobby = Lobby.from_redis(lobby_id)
    if not lobby:
        return jsonify({"error": "Lobby not found"}), 404
    
    # Only the host can update the lobby
    if not lobby.is_host(current_user_id):
        return jsonify({"error": "Only the host can update the lobby"}), 403
    
    # Update the lobby with the provided data
    lobby.update(
        name=data.get('name'),
        max_players=data.get('max_players'),
        status=data.get('status')
    )
    
    # Save the updated lobby
    lobby.save_to_redis()
    
    # Notify all members about the update
    socketio.emit('lobby_updated', lobby.to_dict(), room=f"lobby_{lobby_id}")
    
    return jsonify(lobby.to_dict()), 200

@bp.route("/lobbies/delete-lobby/<lobby_id>", methods=["POST"])
@jwt_required()
def delete_lobby(lobby_id):
    current_user_id = get_jwt_identity()
    
    # Get the lobby from Redis
    lobby = Lobby.from_redis(lobby_id)
    if not lobby:
        return jsonify({"error": "Lobby not found"}), 404
    
    # Only the host can delete the lobby
    if not lobby.is_host(current_user_id):
        return jsonify({"error": "Only the host can delete the lobby"}), 403
    
    # Delete the lobby from Redis
    current_app.redis_client.delete(f"lobby:{lobby_id}")
    
    # Notify all members about the deletion
    socketio.emit('lobby_deleted', {"lobby_id": lobby_id}, room=f"lobby_{lobby_id}")
    
    return jsonify({"message": "Lobby deleted"}), 200

@bp.route("/users/batch", methods=["POST"])
@jwt_required()
def get_users_batch():
    """Get usernames for a batch of user IDs"""
    data = request.get_json() or {}
    user_ids = data.get('user_ids', [])
    
    if not user_ids:
        return jsonify({"users": {}}), 200
    
    # Get users from database
    users = User.query.filter(User.id.in_(user_ids)).all()
    
    # Create a map of user_id -> username
    user_map = {user.id: {"username": user.username} for user in users}
    
    return jsonify({"users": user_map}), 200

@bp.route("/lobbies/start-lobby/<lobby_id>", methods=["POST"])
@jwt_required()
def start_lobby(lobby_id):
    current_user_id = get_jwt_identity()
    
    # Get the lobby from Redis
    lobby = Lobby.from_redis(lobby_id)
    if not lobby:
        return jsonify({"error": "Lobby not found"}), 404
    
    # Only the host can start the lobby
    if not lobby.is_host(current_user_id):
        return jsonify({"error": "Only the host can start the lobby"}), 403
    
    # Check if at least 2 players are in the lobby
    if len(lobby.members) < 2:
        return jsonify({"error": "At least 2 players are required to start"}), 400
    
    # Update lobby status and all member statuses
    lobby.update(status="starting")
    lobby.set_all_members_status("connecting")
    lobby.save_to_redis()
    
    # Notify all members about the lobby starting
    socketio.emit('lobby_starting', lobby.to_dict(), room=f"lobby_{lobby_id}")
    
    # Start a 15-second timer
    @copy_current_request_context
    def check_lobby_status():
        socketio.sleep(15)
        
        # Re-fetch the lobby after 10 seconds
        updated_lobby = Lobby.from_redis(lobby_id)
        if not updated_lobby:
            return
        
        # If lobby is already failed, do nothing
        # TODO: Investigate why this check might not be working?
        if updated_lobby.status == "failed":
            return
        
        # If lobby is still in starting state, stop it
        if updated_lobby.status == "starting":
            updated_lobby.update(status="idle")
            updated_lobby.set_all_members_status("disconnected")
            updated_lobby.save_to_redis()
            
            # Notify all members about the lobby stopping
            socketio.emit('lobby_stopping', updated_lobby.to_dict(), room=f"lobby_{lobby_id}")
            current_app.logger.warning(f"Lobby {lobby_id} stopped due to timeout")
    
    # Start the background task
    socketio.start_background_task(check_lobby_status)
    
    return jsonify({"message": "Starting lobby"}), 200

@bp.route("/lobbies/stop-lobby/<lobby_id>", methods=["POST"])
@jwt_required()
def stop_lobby(lobby_id):
    current_user_id = get_jwt_identity()
    
    # Get the lobby from Redis
    lobby = Lobby.from_redis(lobby_id)
    if not lobby:
        return jsonify({"error": "Lobby not found"}), 404
    
    # Only the host can stop the lobby
    if not lobby.is_host(current_user_id):
        return jsonify({"error": "Only the host can stop the lobby"}), 403
    
    # Update lobby status and all member statuses
    lobby.update(status="idle")
    lobby.set_all_members_status("disconnected")
    lobby.save_to_redis()
    
    # Notify all members about the lobby stopping
    socketio.emit('lobby_stopping', lobby.to_dict(), room=f"lobby_{lobby_id}")
    
    return jsonify({"message": "Stopping lobby"}), 200

@bp.route("/lobbies/<lobby_id>/peer-info", methods=["GET"])
@jwt_required()
def get_peer_info(lobby_id):
    current_user_id = get_jwt_identity()
    
    # Get the lobby from Redis
    lobby = Lobby.from_redis(lobby_id)
    if not lobby:
        return jsonify({"error": "Lobby not found"}), 404
    
    # Check if user is a member of the lobby
    if not lobby.is_member(current_user_id):
        return jsonify({"error": "Not a member of this lobby"}), 403
    
    # Get all online users
    online_users = OnlineUser.get_all_online_users()
    
    # Build peer info list with connection strings and public keys
    peers = []
    # TODO: Maybe remove self_index later.
    self_index = -1
    
    for i, member_id in enumerate(lobby.members):
        if member_id in online_users:
            user_data = online_users[member_id]
            connection_string = f"{user_data['ip']}:{user_data['port']}"
            
            if member_id == current_user_id:
                self_index = i
                peers.append({
                    "stun_info": "self",
                    "public_key": ""  # Empty public key for self
                })
            elif connection_string == "0":
                peers.append({
                    "stun_info": "unavailable",
                    "public_key": ""  # Empty public key for unavailable
                })
            else:
                peers.append({
                    "stun_info": connection_string,
                    "public_key": user_data.get('public_key', '')
                })
        else:
            # User is not online, add placeholder
            peers.append({
                "stun_info": "unavailable",
                "public_key": ""  # Empty public key for unavailable
            })
    
    return jsonify({
        "peers": peers,
        "self_index": self_index
    }), 200