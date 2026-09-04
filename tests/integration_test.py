import urllib.request
import urllib.parse
import json
import time
import sys

BASE_URL = "http://127.0.0.1:8095"

def make_request(path, method="GET", data=None, token=None, content_type="application/json"):
    url = f"{BASE_URL}{path}"
    headers = {}
    if content_type:
        headers["Content-Type"] = content_type
    if token:
        headers["Authorization"] = f"Bearer {token}"
    headers["Accept"] = "application/json"

    encoded_data = None
    if data is not None:
        if isinstance(data, (dict, list)):
            encoded_data = json.dumps(data).encode("utf-8")
        elif isinstance(data, str):
            encoded_data = data.encode("utf-8")
        elif isinstance(data, bytes):
            encoded_data = data

    req = urllib.request.Request(url, data=encoded_data, headers=headers, method=method)
    try:
        with urllib.request.urlopen(req) as resp:
            resp_body = resp.read().decode("utf-8")
            try:
                json_data = json.loads(resp_body)
                return resp.status, json_data
            except:
                return resp.status, resp_body
    except urllib.error.HTTPError as e:
        err_body = e.read().decode("utf-8")
        try:
            json_data = json.loads(err_body)
            return e.code, json_data
        except:
            return e.code, err_body

def test_health():
    status, data = make_request("/api/health")
    assert status == 200
    assert data.get("status") == "ok"
    assert data.get("auth") == True
    print("[PASS] Health check")

def test_auth_workflow():
    # 1. Register Alice
    status, alice_reg = make_request("/api/auth/register", method="POST", data={
        "username": "alice",
        "password": "Password123"
    })
    assert status == 201, f"Alice reg failed: {alice_reg}"
    alice_token = alice_reg.get("token")
    assert alice_token is not None
    print("[PASS] User Alice registered")

    # 2. Register Bob
    status, bob_reg = make_request("/api/auth/register", method="POST", data={
        "username": "bob",
        "password": "PasswordBob123"
    })
    assert status == 201
    bob_token = bob_reg.get("token")
    assert bob_token is not None
    print("[PASS] User Bob registered")

    # 3. Verify /api/auth/me
    status, me_data = make_request("/api/auth/me", token=alice_token)
    assert status == 200
    assert me_data.get("username") == "alice"
    print("[PASS] GET /api/auth/me for Alice")

    # 4. Login with bad password
    status, bad_login = make_request("/api/auth/login", method="POST", data={
        "username": "alice",
        "password": "WrongPassword"
    })
    assert status == 400
    print("[PASS] Login rejected with wrong password")

    # 5. Login with good password
    status, good_login = make_request("/api/auth/login", method="POST", data={
        "username": "alice",
        "password": "Password123"
    })
    assert status == 200
    assert good_login.get("token") is not None
    print("[PASS] Login succeeded for Alice")

    return alice_token, bob_token

def test_paste_privacy_and_isolation(alice_token, bob_token):
    # 1. Alice creates a PUBLIC paste
    status, pub_resp = make_request("/api/pastes", method="POST", token=alice_token, data={
        "title": "Alice Public Note",
        "content": "This is public info from Alice",
        "is_public": True,
        "is_encrypted": False
    })
    assert status == 201
    pub_key = pub_resp.get("key")
    print(f"[PASS] Alice created public paste: {pub_key}")

    # Alice, Bob, and Guest can all read the public paste
    status, res = make_request(f"/api/pastes/{pub_key}", token=alice_token)
    assert status == 200
    assert res.get("content") == "This is public info from Alice"

    status, res = make_request(f"/api/pastes/{pub_key}", token=bob_token)
    assert status == 200
    assert res.get("content") == "This is public info from Alice"

    status, res = make_request(f"/api/pastes/{pub_key}", token=None)
    assert status == 200
    assert res.get("content") == "This is public info from Alice"
    print("[PASS] Public paste accessible by Alice, Bob, and Guest")

    # 2. Alice creates a PRIVATE paste
    status, priv_resp = make_request("/api/pastes", method="POST", token=alice_token, data={
        "title": "Alice Secret Note",
        "content": "TOP SECRET CONTENT - ALICE ONLY",
        "is_public": False,
        "is_encrypted": False
    })
    assert status == 201
    priv_key = priv_resp.get("key")
    print(f"[PASS] Alice created private paste: {priv_key}")

    # Alice (Owner) CAN read it
    status, res = make_request(f"/api/pastes/{priv_key}", token=alice_token)
    assert status == 200
    assert res.get("content") == "TOP SECRET CONTENT - ALICE ONLY"
    assert res.get("is_public") == False
    print("[PASS] Alice successfully retrieved her own private paste")

    # Bob (Other User) CANNOT read it -> 403 Forbidden!
    status, res = make_request(f"/api/pastes/{priv_key}", token=bob_token)
    assert status == 403
    print("[PASS] Bob is forbidden from reading Alice's private paste (403 Forbidden)")

    # Anonymous / Guest CANNOT read it -> 403 Forbidden!
    status, res = make_request(f"/api/pastes/{priv_key}", token=None)
    assert status == 403
    print("[PASS] Guest is forbidden from reading Alice's private paste (403 Forbidden)")

    # 3. Alice lists her pastes
    status, user_pastes = make_request("/api/user/pastes", token=alice_token)
    assert status == 200
    assert len(user_pastes) >= 2
    keys = [p["key"] for p in user_pastes]
    assert pub_key in keys
    assert priv_key in keys
    print("[PASS] Alice's dashboard lists all her public & private pastes")

    # Bob's dashboard does NOT contain Alice's pastes
    status, bob_pastes = make_request("/api/user/pastes", token=bob_token)
    assert status == 200
    bob_keys = [p["key"] for p in bob_pastes]
    assert pub_key not in bob_keys
    assert priv_key not in bob_keys
    print("[PASS] Bob's dashboard is isolated from Alice's pastes")

    # 4. Deletion Permissions
    # Bob tries to delete Alice's private paste -> 404/Failed
    status, del_resp = make_request(f"/api/pastes/{priv_key}", method="DELETE", token=bob_token)
    assert status in [403, 404]

    # Alice can delete her private paste
    status, del_resp = make_request(f"/api/pastes/{priv_key}", method="DELETE", token=alice_token)
    assert status == 200
    assert del_resp.get("status") == "deleted"
    print("[PASS] Alice deleted her paste; unauthorized deletion blocked")

    # Verify deleted
    status, _ = make_request(f"/api/pastes/{priv_key}", token=alice_token)
    assert status == 404
    print("[PASS] Verified paste is no longer accessible after deletion")

def test_encrypted_paste(alice_token):
    # Simulated client-side AES-GCM formatted payload
    encrypted_payload = "ENCRYPTED:v1:0123456789abcdef:fedcba9876543210:abcdef0123456789"
    status, resp = make_request("/api/pastes", method="POST", token=alice_token, data={
        "title": "Encrypted Diary",
        "content": encrypted_payload,
        "is_public": True,
        "is_encrypted": True
    })
    assert status == 201
    enc_key = resp.get("key")

    status, fetch_res = make_request(f"/api/pastes/{enc_key}")
    assert status == 200
    assert fetch_res.get("is_encrypted") == True
    assert fetch_res.get("content") == encrypted_payload
    print(f"[PASS] Encrypted paste {enc_key} stored and retrieved with encryption flag")

if __name__ == "__main__":
    print("Running E2E Integration & Security tests...")
    test_health()
    alice_tok, bob_tok = test_auth_workflow()
    test_paste_privacy_and_isolation(alice_tok, bob_tok)
    test_encrypted_paste(alice_tok)
    print("\nALL E2E INTEGRATION & SECURITY TESTS PASSED!")
