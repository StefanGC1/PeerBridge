import logging
import os
from flask import Flask
from .extensions import db, migrate, socketio, jwt, cors, session
from .routes import bp as api_bp


def create_app():
    app = Flask(__name__)

    # Dev / prod mode
    app.config['ENV'] = os.getenv('FLASK_ENV', 'development')
    if app.config['ENV'] == 'development':
        app.config['DEBUG'] = True

        app.logger.setLevel(logging.DEBUG)
        app.logger.debug('Running in development mode')

    # Config
    db_path = os.getenv('SQLITE_DB_PATH', 'data/my_database.db')
    app.config['SQLALCHEMY_DATABASE_URI'] = f'sqlite:///{db_path}'
    app.config['SQLALCHEMY_TRACK_MODIFICATIONS'] = False

    # Set JWT secret key (change ts later)
    app.config['JWT_SECRET_KEY'] = os.getenv('JWT_SECRET_KEY', 'looogiC')

    # Redis setup
    from redis import Redis
    redis_client = Redis(
        host=os.getenv('REDIS_HOST', 'redis'),
        port=int(os.getenv('REDIS_PORT', 6379)),
        decode_responses=True
    )
    app.redis_client = redis_client

    # Flask-Session related config
    app.config.update(
        SECRET_KEY=os.getenv('SECRET_KEY', 'LoLoLogic'),
        SESSION_TYPE='redis',
        SESSION_PERMANENT=False,
        SESSION_USE_SIGNER=True,
        SESSION_REDIS=redis_client
    )

    # Setup extensions
    db.init_app(app)
    migrate.init_app(app, db)
    jwt.init_app(app)
    cors.init_app(app, supports_credentials=True)
    session.init_app(app)
    socketio.init_app(app, 
                     cors_allowed_origins="*", 
                     ping_timeout=60,
                     ping_interval=15,
                     websocket_timeout=30,
                     async_mode='eventlet',
                     manage_session=True)

    # Necessary for logging in socketIO events, I think
    from . import sockets

    # Register blueprint
    app.register_blueprint(api_bp, url_prefix='/api')

    return app