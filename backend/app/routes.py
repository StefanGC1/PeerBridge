from flask import Blueprint, request, jsonify, current_app
from .extensions import db
from .models import User
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

    if not username or not password:
        return jsonify({'error': 'username and password required'}), 400

    user = User.query.filter_by(username=username).first()
    if not user or not user.check_password(password):
        return jsonify({'error': 'invalid username or password'}), 401

    user_id = user.id
    # Placeholder data
    user_ip = "1.1.1.1"
    user_port = "12345"
    key = f"online:{user_id}"

    # TODO: Move getting redis instance to separate function
    redis_inst = current_app.redis_client

    redis_inst.hset(key, mapping={"ip": user_ip, "port": user_port})
    # Set initial TTL - will be removed when user connects via websocket
    redis_inst.expire(key, 300)  # 5 minutes initial TTL

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
    # TODO: Move getting redis instance to separate function
    redis_inst = current_app.redis_client

    # Example: All Redis keys that match pattern "online:*"
    keys = redis_inst.keys("online:*")
    # For each key, get IP/port data
    online_users = {}
    for key in keys:
        user_id = key.split(":")[1]  # e.g. "online:123" -> user_id = "123"
        user_data = redis_inst.hgetall(key)  # if you're storing IP/port as a hash
        online_users[user_id] = user_data
    
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
    redis_inst = current_app.redis_client
    online_keys = redis_inst.keys("online:*")
    online_user_ids = [key.split(":")[1] for key in online_keys]
    
    # Convert to dictionary and add online status
    friends_list = []
    for friend in friends:
        friend_data = friend.to_dict()
        friend_data['online'] = friend.id in online_user_ids
        if friend.id in online_user_ids:
            friend_data['connection_info'] = redis_inst.hgetall(f"online:{friend.id}")
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