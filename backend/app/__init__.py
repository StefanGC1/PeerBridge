import os
from flask import Flask
from .extensions import db, migrate, socketio, jwt
from .routes import bp as api_bp


def create_app():
    app = Flask(__name__)

    # Config
    db_path = os.getenv('SQLITE_DB_PATH', 'data/my_database.db')
    app.config['SQLALCHEMY_DATABASE_URI'] = f'sqlite:///{db_path}'
    app.config['SQLALCHEMY_TRACK_MODIFICATIONS'] = False

    # Set JWT secret key (change ts later)
    app.config['JWT_SECRET_KEY'] = os.getenv('JWT_SECRET_KEY', 'looogiC')

    # Setup extensions
    db.init_app(app)
    migrate.init_app(app, db)
    socketio.init_app(app, cors_allowed_origins="*")
    jwt.init_app(app)

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