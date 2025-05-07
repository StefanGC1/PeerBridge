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
    # Set TTL
    redis_inst.expire(key, 60)

    access_token = create_access_token(identity=user.id)
    return jsonify({
        "message": f"User {user_id} logged in",
        "access_token": access_token,
        "user_id": user_id
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