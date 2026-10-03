"""Loopback WSS tunnel fixtures; test framing only, never a production WS parser."""
from contextlib import contextmanager
from pathlib import Path
import base64, hashlib, os, socket, socketserver, ssl, struct, subprocess, threading, time

@contextmanager
def echo_backend():
    class Server(socketserver.ThreadingTCPServer):
        allow_reuse_address=True;daemon_threads=True
    class Handler(socketserver.BaseRequestHandler):
        def handle(self):
            self.request.settimeout(3)
            with self.server.guard:
                self.server.connections+=1
                received=bytearray();self.server.payloads.append(received)
            try:
                while True:
                    data=self.request.recv(4096)
                    if not data:return
                    with self.server.guard:received.extend(data)
                    # Deliberately split writes. TCP may coalesce them again;
                    # assertions never rely on OS segmentation or WS counts.
                    for i in range(0,len(data),3):self.request.sendall(data[i:i+3])
            except (OSError,TimeoutError):pass
    server=Server(('127.0.0.1',0),Handler)
    server.guard=threading.Lock();server.connections=0;server.payloads=[]
    worker=threading.Thread(target=server.serve_forever,kwargs={'poll_interval':0.02});worker.start()
    try:yield server
    finally:server.shutdown();server.server_close();worker.join()

def free_port():
    with socket.socket()as s:s.bind(('127.0.0.1',0));return s.getsockname()[1]

@contextmanager
def gateway(executable,certs,backend_port,name='valid'):
    port=free_port()
    args=[str(executable),'--bind','127.0.0.1','--port',str(port),'--backend-port',str(backend_port),'--cert',str(certs/(name+'.pem')),'--key',str(certs/'server.key'),'--origin','https://game.example.test']
    process=subprocess.Popen(args,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
    try:
        until=time.monotonic()+5
        while True:
            if process.poll()is not None:raise AssertionError(process.communicate())
            try:
                with socket.create_connection(('127.0.0.1',port),timeout=.1):break
            except OSError:
                if time.monotonic()>until:raise AssertionError('gateway not listening')
                time.sleep(.01)
        yield port
    finally:
        if process.poll()is None:process.terminate()
        try:out,err=process.communicate(timeout=5)
        except subprocess.TimeoutExpired:process.kill();process.communicate();raise
        assert process.returncode==0,(process.returncode,out,err)

class Peer:
    """Minimal adversarial test peer. Production delegates RFC6455 to Beast."""
    def __init__(self,port,ca,*,path='/play',origins=(),compression=False):
        context=ssl.create_default_context(cafile=str(ca))
        self.socket=context.wrap_socket(socket.create_connection(('127.0.0.1',port),timeout=3),server_hostname='localhost')
        try:
            key=base64.b64encode(os.urandom(16)).decode()
            extra=''.join('Origin: '+origin+'\r\n'for origin in origins)
            if compression:extra+='Sec-WebSocket-Extensions: permessage-deflate\r\n'
            request=(f'GET {path} HTTP/1.1\r\nHost: localhost:{port}\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Version: 13\r\nSec-WebSocket-Key: {key}\r\n{extra}\r\n')
            self.socket.sendall(request.encode());header=b''
            while not header.endswith(b'\r\n\r\n'):
                part=self.socket.recv(1)
                if not part:raise ConnectionError('upgrade rejected')
                header+=part
                assert len(header)<16384
            accept=base64.b64encode(hashlib.sha1((key+'258EAFA5-E914-47DA-95CA-C5AB0DC85B11').encode()).digest())
            assert b' 101 'in header and accept in header and b'permessage-deflate'not in header.lower(),header
        except BaseException:self.socket.close();raise
    def close(self):self.socket.close()
    def send(self,data,*,opcode=2,final=True,masked=True):
        size=len(data);header=bytes([(128 if final else 0)|opcode])
        header+=bytes([(128 if masked else 0)|(size if size<126 else 126 if size<65536 else 127)])
        if size>=126:header+=struct.pack('!H'if size<65536 else '!Q',size)
        if masked:
            mask=os.urandom(4);header+=mask;data=bytes(x^mask[i%4]for i,x in enumerate(data))
        self.socket.sendall(header+data)
    def exact(self,n):
        data=b''
        while len(data)<n:
            part=self.socket.recv(n-len(data))
            if not part:raise ConnectionError('closed')
            data+=part
        return data
    def receive(self):
        a,b=self.exact(2);n=b&127
        if n==126:n=struct.unpack('!H',self.exact(2))[0]
        if n==127:n=struct.unpack('!Q',self.exact(8))[0]
        assert not b&128 and n<=1024*1024
        body=self.exact(n)
        if a&15==8:raise ConnectionError('close frame')
        return a&15,body

def tunnel_matrix(gateway_exe,probe,certs):
    with echo_backend()as backend,gateway(gateway_exe,certs,backend.server_address[1])as port:
        for mode in ['split','bundle','fragment']:
            for origin in [None,'https://game.example.test']:
                args=[str(probe),'localhost',str(port),str(certs/'ca.pem'),mode]+([origin]if origin else [])
                result=subprocess.run(args,capture_output=True,text=True,timeout=10)
                assert result.returncode==0,(mode,origin,result.returncode,result.stderr)
                print(result.stdout.strip(),origin or 'native',flush=True)
        for options in [dict(path='/wrong'),dict(path='/play?ticket=x'),dict(origins=['']),dict(origins=['null']),dict(origins=['https://evil.example.test']),dict(origins=['https://game.example.test']*2)]:
            before=backend.connections
            try:peer=Peer(port,certs/'ca.pem',**options)
            except ConnectionError:pass
            else:peer.close();raise AssertionError(('accepted',options))
            assert backend.connections==before,('rejected upgrade reached backend',options)
        print('path/origin rejection occurs before backend connection',flush=True)
        peer=Peer(port,certs/'ca.pem',compression=True)
        try:
            peer.send(b'abc',final=False);peer.send(b'keep',opcode=9);peer.send(b'def',opcode=0)
            payload=b'';pong=False
            while len(payload)<6 or not pong:
                opcode,data=peer.receive()
                if opcode==10:assert data==b'keep';pong=True
                else:assert opcode==2;payload+=data
            assert payload==b'abcdef'
        finally:peer.close()
        print('fragment + interleaved Ping: binary data ordered, Pong stays control',flush=True)
        for mode in ['text','unmasked','oversized']:
            before=sum(len(p)for p in backend.payloads)
            peer=Peer(port,certs/'ca.pem')
            try:
                peer.send(b'x'*(16385 if mode=='oversized'else 1),opcode=1 if mode=='text'else 2,masked=mode!='unmasked')
                try:peer.receive()
                except (ConnectionError,ssl.SSLError,ConnectionResetError):pass
                else:raise AssertionError(('bad frame accepted',mode))
            finally:peer.close()
            assert sum(len(p)for p in backend.payloads)==before,('bad frame forwarded',mode)
            print('gateway rejects',mode,'before forwarding payload',flush=True)

def native_identity_matrix(gateway_exe,probe,certs,*,before=False):
    env=os.environ.copy();env['TETRIS_CA_FILE']=str(certs/'ca.pem')
    cases=[('valid','localhost',True,0),('conflict','localhost',True,4),('cn-only','localhost',True,0 if before else 4),('ip','127.0.0.1',True,0),('valid','127.0.0.1',True,4),('valid','localhost',False,4),('expired','localhost',True,4),('partial-wildcard','localhost',True,4)]
    for name,host,trusted,expected in cases:
        with echo_backend()as backend,gateway(gateway_exe,certs,backend.server_address[1],name)as port:
            e=env.copy()
            if not trusted:e.pop('TETRIS_CA_FILE',None)
            result=subprocess.run([str(probe),f'wss://{host}:{port}/play'],env=e,capture_output=True,text=True,timeout=10)
            assert result.returncode==expected,(name,host,trusted,result.returncode,result.stderr)
            assert backend.connections==(1 if expected==0 else 0),(name,backend.connections)
            assert b''.join(backend.payloads)==(bytes([1,2,3,4,5])if expected==0 else b'')
            print('native','before'if before else 'after',name,host,trusted,'exit',expected,'backend',backend.connections,flush=True)
