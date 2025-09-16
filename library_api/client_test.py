import requests

base_url = "http://localhost:8000"

# 1) 회원가입 (이메일은 유효해야 함)
signup_data = {
    "username": "john_doe",
    "email": "john@example.com",  # ← 질문 예시의 'john@@...' 오타 수정
    "password": "securepass123",
    "full_name": "John Doe"
}
r = requests.post(f"{base_url}/auth/signup", json=signup_data)
print("signup:", r.status_code, r.json() if r.status_code < 400 else r.text)

# 2) 로그인 (OAuth2 표준: form-encoded)
login_form = {"username": "john_doe", "password": "securepass123"}
auth = requests.post(f"{base_url}/auth/login", data=login_form)
print("login:", auth.status_code, auth.json())
token = auth.json()["access_token"]
headers = {"Authorization": f"Bearer {token}"}

# 3) (관리자) 책 등록 → admin 계정으로 로그인해서 header 생성
admin_login = requests.post(f"{base_url}/auth/login", data={"username": "admin", "password": "admin1234"})
admin_token = admin_login.json()["access_token"]
admin_headers = {"Authorization": f"Bearer {admin_token}"}

book_data = {
    "title": "Python Programming",
    "author": "John Smith",
    "isbn": "978-0123456789",
    "category": "Programming",
    "total_copies": 5
}
r = requests.post(f"{base_url}/books", json=book_data, headers=admin_headers)
print("create book:", r.status_code, r.json())

# 4) 검색 (카테고리=Programming 이면서 대출 가능)
r = requests.get(f"{base_url}/books", params={"category": "Programming", "available": True})
print("search:", r.status_code, r.json()[:1], "... total", len(r.json()))

# 5) 대출 (john_doe가 방금 등록한 책을 빌림)
book_id = r.json()[0]["id"]
borrow = requests.post(f"{base_url}/loans", json={"book_id": book_id}, headers=headers)
print("borrow:", borrow.status_code, borrow.json())

# 6) 내 대출 목록
loans = requests.get(f"{base_url}/users/me/loans", headers=headers)
print("my loans:", loans.status_code, loans.json())

# 7) 반납
loan_id = loans.json()[0]["id"]
ret = requests.post(f"{base_url}/loans/{loan_id}/return", headers=headers)
print("return:", ret.status_code, ret.json())
