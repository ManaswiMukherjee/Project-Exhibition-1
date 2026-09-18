import os
from sqlmodel import SQLModel, create_engine, Session

TURSO_DB_URL = os.getenv("TURSO_DATABASE_URL", "")
TURSO_AUTH_TOKEN = os.getenv("TURSO_AUTH_TOKEN", "")

if TURSO_DB_URL and TURSO_AUTH_TOKEN:
    # Strip existing protocols and parameters to isolate the raw hostname
    clean_host = (
        TURSO_DB_URL
        .replace("sqlite+libsql://", "")
        .replace("libsql://", "")
        .replace("https://", "")
        .replace("http://", "")
        .split("?")[0]
        .strip("/")
    )

    # Use ?secure=true to enforce HTTPS in sqlalchemy-libsql safely
    database_url = f"sqlite+libsql://{clean_host}?secure=true"

    engine = create_engine(
        database_url,
        connect_args={"auth_token": TURSO_AUTH_TOKEN},
        echo=True,
    )
else:
    engine = create_engine("sqlite:///./orders.db", echo=True)

def init_db():
    SQLModel.metadata.create_all(engine)

def get_session():
    with Session(engine) as session:
        yield session