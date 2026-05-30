import requests
import json
import time

BASE = "http://localhost:8080"

def show(name, r):
    print(f"\n=== {name} ===")
    print("status:", r.status_code)
    try:
        print(json.dumps(r.json(), indent=2))
    except Exception:
        print(r.text)

# 1. health
r = requests.get(f"{BASE}/health")
show("GET /health", r)

# 2. register node
node = {
    "node_id": 1,
    "region": "us-east",
    "host": "127.0.0.1",
    "port": 9001,
    "current_load": 10,
    "max_capacity": 100,
    "latency_ms": 20,
}

r = requests.post(f"{BASE}/nodes/register", json=node)
show("POST /nodes/register", r)

# 3. list nodes
r = requests.get(f"{BASE}/nodes")
show("GET /nodes", r)

# 4. list healthy nodes
r = requests.get(f"{BASE}/healthy-nodes")
show("GET /healthy-nodes", r)

# 5. schedule
request = {
    "preferred_region": "us-east",
    "max_latency_ms": 100,
    "allow_cross_region": True,
}

r = requests.post(f"{BASE}/schedule", json=request)
show("POST /schedule", r)

# 6. heartbeat
heartbeat = {
    "node_id": 1,
    "current_load": 30,
    "max_capacity": 100,
    "latency_ms": 25,
}

r = requests.post(f"{BASE}/nodes/heartbeat", json=heartbeat)
show("POST /nodes/heartbeat", r)

# 7. list nodes after heartbeat
r = requests.get(f"{BASE}/nodes")
show("GET /nodes after heartbeat", r)

# 8. wait for timeout
print("\nWaiting 20 seconds to test heartbeat timeout...")
time.sleep(20)

# 9. list nodes after timeout
r = requests.get(f"{BASE}/nodes")
show("GET /nodes after timeout", r)

# 10. schedule after timeout
r = requests.post(f"{BASE}/schedule", json=request)
show("POST /schedule after timeout", r)