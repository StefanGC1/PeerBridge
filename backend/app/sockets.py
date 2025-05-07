from flask import request, current_app
from flask_socketio import emit, join_room, leave_room
from .extensions import socketio

# Some placeholder SocketIO events
@socketio.on('connect')
def handle_connect():
    current_app.logger.info(f'Client connected: {request.sid}')
    emit('connected', {'message': 'You are connected!'})

@socketio.on('disconnect')
def handle_disconnect():
    current_app.logger.info(f'Client disconnected: {request.sid}')

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
