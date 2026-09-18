import os
from sqlmodel import SQLModel, create_engine, Session

TURSO_DB_URL = os.getenv("TURSO_DATABASE_URL")
TURSO_AUTH_TOKEN = os.getenv("TURSO_AUTH_TOKEN")

if TURSO_DB_URL and TURSO_AUTH_TOKEN:
    # Ensure scheme is sqlite+libsql://
    if TURSO_DB_URL.startswith("libsql://"):
        base_url = TURSO_DB_URL.replace("libsql://", "sqlite+libsql://")
    elif TURSO_DB_URL.startswith("https://"):
        base_url = TURSO_DB_URL.replace("https://", "sqlite+libsql://")
    elif not TURSO_DB_URL.startswith("sqlite+libsql://"):
        base_url = f"sqlite+libsql://{TURSO_DB_URL}"
    else:
        base_url = TURSO_DB_URL

    # Strip existing query parameters if present
    base_url = base_url.split("?")[0]

    # Append required secure flag and auth token
    database_url = f"{base_url}?secure=true&authToken={TURSO_AUTH_TOKEN}"
else:
    database_url = "sqlite:///./orders.db"

engine = create_engine(database_url, echo=True)

def init_db():
    SQLModel.metadata.create_all(engine)

def get_session():
    with Session(engine) as session:
        yield session