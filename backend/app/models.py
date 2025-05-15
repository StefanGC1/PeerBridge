from datetime import datetime
import uuid
from werkzeug.security import generate_password_hash, check_password_hash
from .extensions import db

# Association table for user friends (many-to-many relationship)
user_friends = db.Table('user_friends',
    db.Column('user_id', db.String(36), db.ForeignKey('users.id'), primary_key=True),
    db.Column('friend_id', db.String(36), db.ForeignKey('users.id'), primary_key=True)
)

class User(db.Model):
    __tablename__ = 'users'
    id = db.Column(db.String(36), primary_key=True, default=lambda: str(uuid.uuid4()), unique=True, nullable=False)
    username = db.Column(db.String(80), unique=True, nullable=False)
    password_hash = db.Column(db.String(128), nullable=False)
    created_at = db.Column(db.DateTime, default=datetime.utcnow)
    
    # Define the many-to-many relationship for friends
    friends = db.relationship(
        'User',
        secondary=user_friends,
        primaryjoin=(user_friends.c.user_id == id),
        secondaryjoin=(user_friends.c.friend_id == id),
        backref=db.backref('friended_by', lazy='dynamic'),
        lazy='dynamic'
    )

    def set_password(self, password):
        self.password_hash = generate_password_hash(password)

    def check_password(self, password):
        return check_password_hash(self.password_hash, password)
    
    def add_friend(self, user):
        if user not in self.friends:
            self.friends.append(user)
            return True
        return False
    
    def remove_friend(self, user):
        if user in self.friends:
            self.friends.remove(user)
            return True
        return False
    
    def get_friends(self):
        return self.friends.all()
    
    def to_dict(self):
        return {
            'id': self.id,
            'username': self.username,
            'created_at': self.created_at.isoformat() if self.created_at else None
        }