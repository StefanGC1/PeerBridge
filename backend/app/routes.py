from flask import Blueprint, request, jsonify, current_app
from .extensions import db, socketio
from .models import User
from .schemas import OnlineUser, Lobby
from flask_jwt_extended import (
    create_access_token,
    jwt_required,
    get_jwt_identity
)

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

    return jsonify({'message': 'user registered', 'user_id': user.id}), 201

@bp.route("/login", methods=["POST"])
def login():
    data = request.get_json() or {}
    username = data.get('username')
    password = data.get('password')
    
    # Get STUN info from request with better error handling
    try:
        user_ip = data.get('public_ip', '1.1.1.1')  # Default if not provided
        user_port = int(data.get('public_port', 12345))  # Convert to int and use default if not provided
    except (ValueError, TypeError) as e:
        # Handle case where port is not a valid integer
        current_app.logger.warning(f"Invalid STUN port format: {e}. Using default.")
        user_ip = data.get('public_ip', '1.1.1.1')
        user_port = 12345  # Use default

    if not username or not password:
        return jsonify({'error': 'username and password required'}), 400

    user = User.query.filter_by(username=username).first()
    if not user or not user.check_password(password):
        return jsonify({'error': 'invalid username or password'}), 401

    user_id = user.id

    # Print STUN info to console
    current_app.logger.debug(f"User {username} (ID: {user_id}) logged in with STUN info - IP: {user_ip}, Port: {user_port}")

    # Store user in Redis using OnlineUser class
    online_user = OnlineUser(
        user_id=user_id,
        ip=user_ip,
        port=user_port
    )
    online_user.save_to_redis(expire_seconds=300)  # 5 minutes initial TTL

    access_token = create_access_token(identity=user.id)
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
        status='open'
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
    
    # Get the lobby from Redis
    lobby = Lobby.from_redis(lobby_id)
    if not lobby:
        return jsonify({"error": "Lobby not found"}), 404
    
    # Check if lobby is full
    if lobby.is_full():
        return jsonify({"error": "Lobby is full"}), 400
    
    # Check if lobby is closed
    if lobby.status != 'open':
        return jsonify({"error": "Lobby is not open for joining"}), 400
    
    # Check if user is already in the lobby
    if lobby.is_member(current_user_id):
        return jsonify({"error": "Already in this lobby"}), 400
    
    # Add user to lobby
    lobby.add_member(current_user_id)
    lobby.save_to_redis()
    
    # Notify all members about the update
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