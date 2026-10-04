"""Create and inspect one loopback fixture account; keep its secret in memory."""
import json, sys, urllib.request
port = int(sys.argv[1])
if not 1 <= port <= 65535:
    raise SystemExit("port out of range")
base = f"http://127.0.0.1:{port}"
request = urllib.request.Request(base + "/study/v1/guest", b"{}", {"Content-Type": "application/json"})
with urllib.request.urlopen(request, timeout=5) as response:
    account = json.load(response)
request = urllib.request.Request(base + "/study/v1/me", headers={"Authorization": "Bearer " + account["token"]})
with urllib.request.urlopen(request, timeout=5) as response:
    profile = json.load(response)
if profile["player_id"] != account["player_id"]:
    raise SystemExit("account mismatch")
print("Own profile:", profile)
# Deliberately no credential file: run the same process to repeat authenticated reads.
