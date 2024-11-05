import random
import time
from flask import Flask, request
from flask_socketio import SocketIO, join_room, leave_room, emit
from settings import config

app = Flask(__name__)
app.config.from_object(config)

# Initialize Flask-SocketIO with gevent
socketio = SocketIO(app, async_mode="gevent")

# TODO: Change way peers are stored
connected_peers = {}

@app.route('/')
def index():
    return "PeerBridge Backend Server is Running!"

# Socket event handler for when a client connects
@socketio.on('connect')
def handle_connect():
    print(f"Client {request.sid} connected.")
    emit('connected', {'message': 'Successfully connected to the server'})

# Register a peer and add them to the list
@socketio.on('register')
def handle_register(data):
    username = data.get('username')
    ip_address = request.remote_addr
    port = data.get('port')
    connected_peers[request.sid] = {'username': username, 'ip': ip_address, 'port': port}
    
    # Notify other clients of the new peer
    emit('new_peer', connected_peers[request.sid], broadcast=True)
    print(f"Registered peer {username} with IP {ip_address} and port {port}")

# Handle peer discovery request
@socketio.on('discover_peers')
def handle_discover_peers():
    emit('peer_list', list(connected_peers.values()))

# Handle disconnection and cleanup
@socketio.on('disconnect')
def handle_disconnect():
    peer = connected_peers.pop(request.sid, None)
    if peer:
        emit('peer_disconnected', peer, broadcast=True)
    print(f"Client {request.sid} disconnected.")

# TEST: Background task to emit a random number every 5 seconds
def random_number_task():
    while True:
        print("Sending rand num")
        random_number = random.randint(1, 100)  # Generate a random number
        socketio.emit('rand-num', {'number': random_number})
        socketio.sleep(5)  # Wait for 5 seconds before sending the next number

if __name__ == '__main__':
    socketio.start_background_task(random_number_task)
    socketio.run(app, host='0.0.0.0', port=5000)
