import os
from sqlmodel import SQLModel, create_engine, Session

# Pull Turso credentials from environment variables
TURSO_DB_URL = os.getenv("TURSO_DATABASE_URL")
TURSO_AUTH_TOKEN = os.getenv("TURSO_AUTH_TOKEN")

if TURSO_DB_URL and TURSO_AUTH_TOKEN:
    # Convert 'libsql://' or 'https://' to 'sqlite+libsql://' for SQLAlchemy
    clean_url = TURSO_DB_URL.replace("libsql://", "").replace("https://", "")
    database_url = f"sqlite+libsql://{clean_url}?auth_token={TURSO_AUTH_TOKEN}"
else:
    # Fallback to local SQLite file for local dev
    database_url = "sqlite:///./orders.db"

engine = create_engine(database_url, echo=True)

def init_db():
    SQLModel.metadata.create_all(engine)

def get_session():
    with Session(engine) as session:
        yield session