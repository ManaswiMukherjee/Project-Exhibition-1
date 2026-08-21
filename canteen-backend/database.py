from sqlmodel import SQLModel, Session, create_engine

# Single local SQLite file. This whole system is meant to run on a machine
# physically at the canteen (Raspberry Pi / mini-PC / old laptop) on the
# local network -- no cloud DB, no internet dependency for core operation.
DATABASE_URL = "sqlite:///./orders.db"

# check_same_thread=False is required because FastAPI can handle requests
# on different threads, but we're still only ever hitting a single SQLite
# file on one machine, which is exactly what SQLite is fine with.
engine = create_engine(DATABASE_URL, echo=False, connect_args={"check_same_thread": False})


def init_db() -> None:
    """Create tables if they don't already exist. Safe to call every startup."""
    SQLModel.metadata.create_all(engine)


def get_session():
    """FastAPI dependency that yields a DB session per-request."""
    with Session(engine) as session:
        yield session
