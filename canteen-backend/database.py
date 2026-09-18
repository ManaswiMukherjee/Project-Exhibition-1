import os
from sqlmodel import SQLModel, create_engine, Session

TURSO_DB_URL = os.getenv("TURSO_DATABASE_URL", "")
TURSO_AUTH_TOKEN = os.getenv("TURSO_AUTH_TOKEN", "")

if TURSO_DB_URL and TURSO_AUTH_TOKEN:
    # Extract raw hostname without protocols or query parameters
    clean_host = (
        TURSO_DB_URL.replace("sqlite+libsql://", "")
        .replace("libsql://", "")
        .replace("https://", "")
        .replace("http://", "")
        .split("?")[0]
        .strip("/")
    )

    # Expressly pass https:// inside the sqlite+libsql dialect URL
    database_url = f"sqlite+libsql://https://{clean_host}"

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