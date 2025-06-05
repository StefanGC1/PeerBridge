import logging
import os
from flask import Flask
from .extensions import db, migrate, socketio, jwt, cors
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

    # Setup extensions
    db.init_app(app)
    migrate.init_app(app, db)
    socketio.init_app(app, 
                     cors_allowed_origins="*", 
                     ping_timeout=60,
                     ping_interval=25,
                     async_mode='eventlet',
                     websocket_timeout=30)
    jwt.init_app(app)
    cors.init_app(app, supports_credentials=True)

    from . import sockets
    # Redis setup
    from redis import Redis
    app.redis_client = Redis(
        host=os.getenv('REDIS_HOST', 'redis'),
        port=int(os.getenv('REDIS_PORT', 6379)),
        decode_responses=True
    )

    # Register blueprint
    app.register_blueprint(api_bp, url_prefix='/api')

    return app