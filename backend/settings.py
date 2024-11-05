
import os

class Config:
    SECRET_KEY = os.environ.get('SECRET_KEY') or 'a_very_secret_key'
    SOCKETIO_MESSAGE_QUEUE = None

config = Config()