"""Loopback lifetime experiments; all delays and deadlines are test conditions."""
from contextlib import contextmanager
import json,socket,socketserver,ssl,subprocess,threading
from learning_wss_fixture import echo_backend,gateway

@contextmanager
def stall_server(certs,stage):
    """TCP accepts but never answers TLS, or finishes TLS but never answers Upgrade."""
    stop=threading.Event();reached=threading.Event()
    context=ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.load_cert_chain(str(certs/'valid.pem'),str(certs/'server.key'))
    class Server(socketserver.ThreadingTCPServer):
        allow_reuse_address=True;daemon_threads=True
        def handle_error(self,*args):pass
    class Handler(socketserver.BaseRequestHandler):
        def handle(self):
            connection=self.request;connection.settimeout(2)
            try:
                if stage=='upgrade':
                    connection=context.wrap_socket(connection,server_side=True)
                    data=b''
                    while b'\r\n\r\n' not in data and len(data)<16384:
                        chunk=connection.recv(1024)
                        if not chunk:return
                        data+=chunk
                    if b'\r\n\r\n' not in data:return
                else:
                    # Seeing ClientHello bytes proves the attempted TLS stage.
                    if not connection.recv(1):return
                reached.set();stop.wait(3)
            except (OSError,TimeoutError):pass
            finally:connection.close()
    server=Server(('127.0.0.1',0),Handler)
    worker=threading.Thread(target=server.serve_forever,kwargs={'poll_interval':0.02});worker.start()
    try:yield server.server_address[1],reached
    finally:stop.set();server.shutdown();server.server_close();worker.join()

@contextmanager
def silent_backend(reply=None):
    reached=threading.Event();stop=threading.Event()
    class Server(socketserver.ThreadingTCPServer):
        allow_reuse_address=True;daemon_threads=True
    class Handler(socketserver.BaseRequestHandler):
        def handle(self):
            self.request.settimeout(2)
            try:
                if self.request.recv(1024):
                    reached.set()
                    if reply is not None:self.request.sendall(reply)
                    stop.wait(3)
            except OSError:pass
    server=Server(('127.0.0.1',0),Handler)
    worker=threading.Thread(target=server.serve_forever,kwargs={'poll_interval':0.02});worker.start()
    try:yield server.server_address[1],reached
    finally:stop.set();server.shutdown();server.server_close();worker.join()

def matrix(probe,gateway_exe,certs):
    def invoke(port,mode='none',deadline=1500,expected=0,stage=None,host='localhost'):
        r=subprocess.run([str(probe),host,str(port),str(certs/'ca.pem'),mode,str(deadline)],text=True,capture_output=True,timeout=6)
        assert r.returncode==0,(mode,r.returncode,r.stdout,r.stderr)
        result=json.loads(r.stdout)
        assert result['end']==expected,(mode,result)
        assert result['reports']==1 and result['destroyed']==1 and result['expired'] and result['unrelated'],result
        assert result['late']>=1,result
        if stage is not None:assert result['stage']==stage,result
        print('async',mode,'end',expected,'stage',result['stage'],'late',result['late'],'released/shared-loop alive',flush=True)
        return result
    with echo_backend()as backend,gateway(gateway_exe,certs,backend.server_address[1])as port:
        invoke(port,stage=6)
        for i,mode in enumerate(['resolve','connect','tls','upgrade','write','read'],1):
            invoke(port,mode,expected=1,stage=i)
        invoke(port,'immediate',expected=1)
    for mode,stage in [('tls',3),('upgrade',4)]:
        with stall_server(certs,mode)as(port,reached):
            invoke(port,deadline=350,expected=2,stage=stage)
            assert reached.is_set(),mode
    with silent_backend()as(backend,reached),gateway(gateway_exe,certs,backend)as port:
        invoke(port,deadline=350,expected=2,stage=6)
        assert reached.is_set()
    with silent_backend(b'\x00')as(backend,reached),gateway(gateway_exe,certs,backend)as port:
        invoke(port,expected=4,stage=6)
        assert reached.is_set()
    with echo_backend()as backend,gateway(gateway_exe,certs,backend.server_address[1],'conflict')as port:
        invoke(port,expected=3,stage=3)
        assert backend.connections==0
    # Keep the port bound without listening, avoiding a free-port reuse race.
    with socket.socket()as bound:
        bound.bind(('127.0.0.1',0))
        invoke(bound.getsockname()[1],expected=3,stage=2,host='127.0.0.1')
