# app.py
from datetime import datetime, timedelta
from typing import Optional, List

from fastapi import FastAPI, Depends, HTTPException, status, Query
from fastapi.security import OAuth2PasswordBearer, OAuth2PasswordRequestForm
from jose import jwt, JWTError
from passlib.context import CryptContext
from pydantic import BaseModel, EmailStr, Field, constr
from pydantic_settings import BaseSettings
from sqlalchemy import (
    create_engine, Integer, String, Boolean, DateTime, ForeignKey, func
)
from sqlalchemy.orm import sessionmaker, DeclarativeBase, Mapped, mapped_column, relationship, Session

# -------------------- Settings (.env) --------------------
class Settings(BaseSettings):
    SECRET_KEY: str
    ACCESS_TOKEN_EXPIRE_MINUTES: int = 60
    DATABASE_URL: str = "sqlite:///./library.db"
    ADMIN_DEFAULT_USERNAME: str = "admin"
    ADMIN_DEFAULT_PASSWORD: str = "admin1234"

    class Config:
        env_file = ".env"

settings = Settings()
ALGORITHM = "HS256"

# -------------------- DB --------------------
class Base(DeclarativeBase): ...
engine = create_engine(settings.DATABASE_URL, connect_args={"check_same_thread": False} if settings.DATABASE_URL.startswith("sqlite") else {})
SessionLocal = sessionmaker(bind=engine, autoflush=False, autocommit=False)

# -------------------- Password / JWT --------------------
pwd_context = CryptContext(schemes=["bcrypt"], deprecated="auto")
oauth2_scheme = OAuth2PasswordBearer(tokenUrl="/auth/login")

def get_db():
    db = SessionLocal()
    try:
        yield db
    finally:
        db.close()

def hash_pw(p: str) -> str:
    return pwd_context.hash(p)

def verify_pw(p: str, hashed: str) -> bool:
    return pwd_context.verify(p, hashed)

def create_access_token(sub: str, expires_minutes: int = settings.ACCESS_TOKEN_EXPIRE_MINUTES) -> str:
    to_encode = {"sub": sub, "exp": datetime.utcnow() + timedelta(minutes=expires_minutes)}
    return jwt.encode(to_encode, settings.SECRET_KEY, algorithm=ALGORITHM)

# -------------------- ORM Models --------------------
class User(Base):
    __tablename__ = "users"
    id: Mapped[int] = mapped_column(Integer, primary_key=True, index=True)
    username: Mapped[str] = mapped_column(String(50), unique=True, index=True)
    email: Mapped[str] = mapped_column(String(255), unique=True)
    full_name: Mapped[Optional[str]] = mapped_column(String(255))
    password_hash: Mapped[str] = mapped_column(String(255))
    is_admin: Mapped[bool] = mapped_column(Boolean, default=False)
    created_at: Mapped[datetime] = mapped_column(DateTime, default=func.now())

    loans: Mapped[List["Loan"]] = relationship(back_populates="user")

class Book(Base):
    __tablename__ = "books"
    id: Mapped[int] = mapped_column(Integer, primary_key=True, index=True)
    title: Mapped[str] = mapped_column(String(255), index=True)
    author: Mapped[str] = mapped_column(String(255), index=True)
    isbn: Mapped[str] = mapped_column(String(32), unique=True, index=True)
    category: Mapped[Optional[str]] = mapped_column(String(100), index=True)
    total_copies: Mapped[int] = mapped_column(Integer, default=1)
    available_copies: Mapped[int] = mapped_column(Integer, default=1)
    created_at: Mapped[datetime] = mapped_column(DateTime, default=func.now())

    loans: Mapped[List["Loan"]] = relationship(back_populates="book")

class Loan(Base):
    __tablename__ = "loans"
    id: Mapped[int] = mapped_column(Integer, primary_key=True)
    user_id: Mapped[int] = mapped_column(ForeignKey("users.id"))
    book_id: Mapped[int] = mapped_column(ForeignKey("books.id"))
    borrowed_at: Mapped[datetime] = mapped_column(DateTime, default=func.now())
    returned_at: Mapped[Optional[datetime]] = mapped_column(DateTime, nullable=True)

    user: Mapped[User] = relationship(back_populates="loans")
    book: Mapped[Book] = relationship(back_populates="loans")

Base.metadata.create_all(engine)

# bootstrap admin
with SessionLocal() as db:
    if not db.query(User).filter(User.username == settings.ADMIN_DEFAULT_USERNAME).first():
        db.add(User(
            username=settings.ADMIN_DEFAULT_USERNAME,
            email=f"{settings.ADMIN_DEFAULT_USERNAME}@local",
            full_name="Admin",
            password_hash=hash_pw(settings.ADMIN_DEFAULT_PASSWORD),
            is_admin=True
        ))
        db.commit()

# -------------------- Schemas (Pydantic) --------------------
ISBN = constr(strip_whitespace=True, pattern=r"^[0-9Xx\-]{10,17}$")

class Token(BaseModel):
    access_token: str
    token_type: str = "bearer"

class SignupIn(BaseModel):
    username: constr(strip_whitespace=True, min_length=3, max_length=50)
    email: EmailStr
    password: constr(min_length=8, max_length=128)
    full_name: Optional[constr(max_length=255)] = None

class UserOut(BaseModel):
    id: int
    username: str
    email: EmailStr
    full_name: Optional[str]
    is_admin: bool
    class Config: orm_mode = True

class BookCreate(BaseModel):
    title: constr(min_length=1, max_length=255)
    author: constr(min_length=1, max_length=255)
    isbn: ISBN
    category: Optional[constr(max_length=100)] = None
    total_copies: int = Field(ge=1, default=1)

class BookOut(BaseModel):
    id: int
    title: str
    author: str
    isbn: str
    category: Optional[str]
    total_copies: int
    available_copies: int
    class Config: orm_mode = True

class BorrowIn(BaseModel):
    book_id: int

class LoanOut(BaseModel):
    id: int
    user_id: int
    book_id: int
    borrowed_at: datetime
    returned_at: Optional[datetime]
    class Config: orm_mode = True

# -------------------- Auth helpers --------------------
def get_current_user(token: str = Depends(oauth2_scheme), db: Session = Depends(get_db)) -> User:
    credentials_exception = HTTPException(status_code=status.HTTP_401_UNAUTHORIZED, detail="Could not validate credentials")
    try:
        payload = jwt.decode(token, settings.SECRET_KEY, algorithms=[ALGORITHM])
        username: str = payload.get("sub")  # type: ignore
    except JWTError:
        raise credentials_exception
    user = db.query(User).filter(User.username == username).first()
    if not user:
        raise credentials_exception
    return user

def require_admin(user: User = Depends(get_current_user)) -> User:
    if not user.is_admin:
        raise HTTPException(status_code=403, detail="Admin required")
    return user

# -------------------- FastAPI --------------------
app = FastAPI(title="RGT Library API", version="1.0.0")

# 1) 인증/인가
@app.post("/auth/signup", response_model=UserOut, status_code=201)
def signup(data: SignupIn, db: Session = Depends(get_db)):
    if db.query(User).filter((User.username == data.username) | (User.email == data.email)).first():
        raise HTTPException(400, "username or email already exists")
    user = User(
        username=data.username,
        email=data.email,
        full_name=data.full_name,
        password_hash=hash_pw(data.password),
        is_admin=False
    )
    db.add(user)
    db.commit()
    db.refresh(user)
    return user

@app.post("/auth/login", response_model=Token)
def login(form: OAuth2PasswordRequestForm = Depends(), db: Session = Depends(get_db)):
    user = db.query(User).filter(User.username == form.username).first()
    if not user or not verify_pw(form.password, user.password_hash):
        raise HTTPException(401, "invalid credentials")
    return {"access_token": create_access_token(user.username)}

@app.get("/users/me", response_model=UserOut)
def me(user: User = Depends(get_current_user)):
    return user

@app.get("/users/me/loans", response_model=List[LoanOut])
def my_loans(user: User = Depends(get_current_user), db: Session = Depends(get_db)):
    return db.query(Loan).filter(Loan.user_id == user.id).order_by(Loan.id.desc()).all()

# 2) 도서 관리 (Admin)
@app.post("/books", response_model=BookOut, status_code=201)
def create_book(payload: BookCreate, db: Session = Depends(get_db), admin: User = Depends(require_admin)):
    if db.query(Book).filter(Book.isbn == payload.isbn).first():
        raise HTTPException(400, "ISBN already exists")
    book = Book(
        title=payload.title, author=payload.author, isbn=payload.isbn,
        category=payload.category, total_copies=payload.total_copies,
        available_copies=payload.total_copies
    )
    db.add(book); db.commit(); db.refresh(book)
    return book

@app.get("/books", response_model=List[BookOut])
def list_books(
    category: Optional[str] = Query(None),
    available: Optional[bool] = Query(None),
    q: Optional[str] = Query(None, description="title/author contains"),
    db: Session = Depends(get_db)
):
    query = db.query(Book)
    if category:
        query = query.filter(Book.category == category)
    if available is True:
        query = query.filter(Book.available_copies > 0)
    if q:
        like = f"%{q}%"
        query = query.filter((Book.title.ilike(like)) | (Book.author.ilike(like)))
    return query.order_by(Book.id.asc()).all()

@app.delete("/books/{book_id}", status_code=204)
def delete_book(book_id: int, db: Session = Depends(get_db), admin: User = Depends(require_admin)):
    book = db.get(Book, book_id)
    if not book: raise HTTPException(404, "book not found")
    if db.query(Loan).filter(Loan.book_id == book_id, Loan.returned_at.is_(None)).count():
        raise HTTPException(409, "book has active loans")
    db.delete(book); db.commit()
    return

# 3) 대출/반납
@app.post("/loans", response_model=LoanOut, status_code=201)
def borrow(data: BorrowIn, user: User = Depends(get_current_user), db: Session = Depends(get_db)):
    book = db.get(Book, data.book_id)
    if not book: raise HTTPException(404, "book not found")
    if book.available_copies <= 0:
        raise HTTPException(409, "no available copies")
    # 이미 대출 중인지 여부는 정책에 따라: 여기선 중복 대출 허용 X
    active = db.query(Loan).filter(Loan.user_id == user.id, Loan.book_id == book.id, Loan.returned_at.is_(None)).first()
    if active:
        raise HTTPException(409, "already borrowed")
    book.available_copies -= 1
    loan = Loan(user_id=user.id, book_id=book.id)
    db.add(loan); db.commit(); db.refresh(loan)
    return loan

@app.post("/loans/{loan_id}/return", response_model=LoanOut)
def return_book(loan_id: int, user: User = Depends(get_current_user), db: Session = Depends(get_db)):
    loan = db.get(Loan, loan_id)
    if not loan: raise HTTPException(404, "loan not found")
    if loan.user_id != user.id and not user.is_admin:
        raise HTTPException(403, "not your loan")
    if loan.returned_at is not None:
        raise HTTPException(409, "already returned")
    loan.returned_at = datetime.utcnow()
    book = db.get(Book, loan.book_id)
    book.available_copies += 1
    db.commit(); db.refresh(loan)
    return loan
