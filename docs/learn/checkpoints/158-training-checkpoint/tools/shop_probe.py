"""Read an authenticated inventory and observe a denied purchase without printing secrets."""
import json,sys,urllib.request,urllib.error
port=int(sys.argv[1])
if not 1<=port<=65535:raise SystemExit("port out of range")
base=f"http://127.0.0.1:{port}"
def call(path,body=None,token=None):
    headers={"Content-Type":"application/json"}
    if token:headers["Authorization"]="Bearer "+token
    data=None if body is None else json.dumps(body).encode()
    req=urllib.request.Request(base+path,data,headers)
    try:
        with urllib.request.urlopen(req,timeout=5) as r:return r.status,json.load(r)
    except urllib.error.HTTPError as e:return e.code,json.load(e)
status,account=call("/study/v1/guest",{})
if status!=201:raise SystemExit("registration failed")
token=account["token"]
status,view=call("/study/v1/inventory",token=token)
if status!=200:raise SystemExit("inventory failed")
print("Inventory:",view)
status,result=call("/study/v1/icons/buy",{"icon_id":"ruby"},token)
print("Purchase:",status,result)
if status!=402:raise SystemExit("expected insufficient BP for a new account")
