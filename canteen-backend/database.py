import os
from sqlmodel import SQLModel, create_engine, Session

TURSO_DB_URL = os.getenv("TURSO_DATABASE_URL", "")
TURSO_AUTH_TOKEN = os.getenv("TURSO_AUTH_TOKEN", "")

if TURSO_DB_URL and TURSO_AUTH_TOKEN:
    # Standardize protocol to sqlite+libsql://
    clean_url = TURSO_DB_URL
    if clean_url.startswith("libsql://"):
        clean_url = clean_url.replace("libsql://", "sqlite+libsql://")
    elif clean_url.startswith("https://"):
        clean_url = clean_url.replace("https://", "sqlite+libsql://")
    elif not clean_url.startswith("sqlite+libsql://"):
        clean_url = f"sqlite+libsql://{clean_url}"

    # Strip existing query params to prevent conflicts
    clean_url = clean_url.split("?")[0]

    engine = create_engine(
        clean_url,
        connect_args={"auth_token": TURSO_AUTH_TOKEN},
        echo=True
    )
else:
    engine = create_engine("sqlite:///./orders.db", echo=True)

def init_db():
    SQLModel.metadata.create_all(engine)

def get_session():
    with Session(engine) as session:
        yield session