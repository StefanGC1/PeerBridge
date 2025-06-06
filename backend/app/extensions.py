from flask_sqlalchemy import SQLAlchemy
from flask_migrate import Migrate
from flask_socketio import SocketIO
from flask_jwt_extended import JWTManager
from flask_cors import CORS
from flask_session import Session

db = SQLAlchemy()
migrate = Migrate()
socketio = SocketIO()
jwt = JWTManager()
cors = CORS()
session = Session()