import socketio

# Initialize Socket.IO client
sio = socketio.Client()

@sio.event
def connect():
    print("Connected to the server!")
    sio.emit('register', {'username': 'UserA', 'port': 12345})

@sio.event
def connected(data):
    print(data['message'])

@sio.event
def new_peer(data):
    print("New peer connected:", data)

@sio.event
def peer_list(data):
    print("Connected peers:", data)

@sio.event
def peer_disconnected(data):
    print("Peer disconnected:", data)

@sio.event
def disconnect():
    print("Disconnected from server")

# Attempt to connect using only WebSocket transport
sio.connect('http://localhost:5000', transports=['websocket'])
sio.wait()
