# Pastebin REST API Specification

Base URL: `http://localhost:8080`

---

## 1. Authentication Endpoints

### Register User
- **Method**: `POST`
- **Path**: `/api/auth/register`
- **Body**: `{"username": "alice", "password": "Password123"}`
- **Response (`201 Created`)**:
  ```json
  {
    "token": "a1b2c3...",
    "user": { "id": 1, "username": "alice" }
  }
  ```

### Log In
- **Method**: `POST`
- **Path**: `/api/auth/login`
- **Body**: `{"username": "alice", "password": "Password123"}`
- **Response (`200 OK`)**:
  ```json
  {
    "token": "a1b2c3...",
    "user": { "id": 1, "username": "alice" }
  }
  ```

### Current User Profile
- **Method**: `GET`
- **Path**: `/api/auth/me`
- **Headers**: `Authorization: Bearer <token>`
- **Response (`200 OK`)**:
  ```json
  {
    "id": 1,
    "username": "alice",
    "created_at": "2026-09-02 15:30:00"
  }
  ```

---

## 2. Paste Endpoints

### Create Paste
- **Method**: `POST`
- **Path**: `/api/pastes`
- **Headers**:
  - `Content-Type: application/json` (or `text/plain`)
  - `Authorization: Bearer <token>` *(optional, enables Private pastes)*
- **Body**:
  ```json
  {
    "title": "My Secret Code",
    "content": "Secret content or AES-GCM ciphertext",
    "is_public": false,
    "is_encrypted": false
  }
  ```
- **Response (`201 Created`)**:
  ```json
  {
    "key": "a7K3xQ",
    "is_public": false,
    "is_encrypted": false
  }
  ```

### Retrieve Paste
- **Method**: `GET`
- **Path**: `/api/pastes/{key}`
- **Headers**:
  - `Authorization: Bearer <token>` *(required if paste is Private)*
  - `Accept: application/json`
- **Response (`200 OK`)**:
  ```json
  {
    "key": "a7K3xQ",
    "title": "My Secret Code",
    "content": "Secret content",
    "is_public": false,
    "is_encrypted": false,
    "created_at": "2026-09-02 15:30:00"
  }
  ```
- **Response (`403 Forbidden`)** *(if private and user is not owner)*:
  ```json
  {
    "error": "This paste is private. Only the author can access it."
  }
  ```

### List User Pastes
- **Method**: `GET`
- **Path**: `/api/user/pastes`
- **Headers**: `Authorization: Bearer <token>`
- **Response (`200 OK`)**:
  ```json
  [
    {
      "key": "a7K3xQ",
      "title": "My Secret Code",
      "is_public": false,
      "is_encrypted": false,
      "created_at": "2026-09-02 15:30:00",
      "snippet": "Secret content..."
    }
  ]
  ```

### Delete Paste
- **Method**: `DELETE`
- **Path**: `/api/pastes/{key}`
- **Headers**: `Authorization: Bearer <token>`
- **Response (`200 OK`)**:
  ```json
  {
    "status": "deleted",
    "key": "a7K3xQ"
  }
  ```
