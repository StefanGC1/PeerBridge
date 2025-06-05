import eventlet
eventlet.monkey_patch()

import os
from app import create_app
from app.extensions import socketio

app = create_app()

if __name__ == '__main__':
    # Werkzeug has severe schizophrenia and thinks it's on prod 
    socketio.run(
        app,
        host='0.0.0.0',
        port=5000, debug=True,
        use_reloader=True,
        allow_unsafe_werkzeug=True)