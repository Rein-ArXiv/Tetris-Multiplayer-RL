"""Loopback-only TLS fixtures with disposable, locally generated test keys."""
from contextlib import contextmanager
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import json, os, ssl, subprocess, threading

def command(args):
    subprocess.run(args, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

def certificates(folder):
    folder = Path(folder); folder.mkdir(parents=True, exist_ok=True)
    command(['openssl','req','-x509','-newkey','ec','-pkeyopt','ec_paramgen_curve:P-256','-nodes','-days','2','-subj','/CN=Study135 test CA','-addext','basicConstraints=critical,CA:TRUE','-addext','keyUsage=critical,keyCertSign,cRLSign','-keyout',str(folder/'ca.key'),'-out',str(folder/'ca.pem')])
    command(['openssl','genpkey','-algorithm','EC','-pkeyopt','ec_paramgen_curve:P-256','-out',str(folder/'server.key')])
    variants = [('valid','unused.invalid','DNS:localhost'),('conflict','localhost','DNS:wrong.invalid'),('cn-only','localhost',None),('ip','unused.invalid','IP:127.0.0.1'),('expired','unused.invalid','DNS:localhost'),('partial-wildcard','unused.invalid','DNS:local*')]
    for serial,(name,cn,san) in enumerate(variants,1):
        csr=folder/(name+'.csr'); cert=folder/(name+'.pem');ext=folder/(name+'.ext')
        command(['openssl','req','-new','-key',str(folder/'server.key'),'-subj','/CN='+cn,'-out',str(csr)])
        ext.write_text('basicConstraints=critical,CA:FALSE\nkeyUsage=critical,digitalSignature\nextendedKeyUsage=serverAuth\n'+('subjectAltName='+san+'\n' if san else ''))
        if name!='expired':
            command(['openssl','x509','-req','-in',str(csr),'-CA',str(folder/'ca.pem'),'-CAkey',str(folder/'ca.key'),'-set_serial',str(serial),'-days','1','-extfile',str(ext),'-out',str(cert)])
        else:
            (folder/'index').write_text('');(folder/'serial').write_text('10\n')
            conf=folder/'ca.cnf';conf.write_text('[ca]\ndefault_ca=test\n[test]\n'+f'database={folder}/index\nserial={folder}/serial\nnew_certs_dir={folder}\ncertificate={folder}/ca.pem\nprivate_key={folder}/ca.key\ndefault_md=sha256\npolicy=policy\n[policy]\ncommonName=supplied\n')
            command(['openssl','ca','-batch','-config',str(conf),'-in',str(csr),'-startdate','20200101000000Z','-enddate','20200102000000Z','-extfile',str(ext),'-out',str(cert)])
    return folder

@contextmanager
def server(folder, name='valid', *, status=200, location='', plain=False):
    calls=[]
    class Handler(BaseHTTPRequestHandler):
        def log_message(self,*args): pass
        def respond(self):
            calls.append((self.command,self.path))
            length=int(self.headers.get('Content-Length','0'));self.rfile.read(length)
            if self.path=='/v1/guest':
                body=json.dumps(dict(player_id=135, token='a'*32, elo=0,bp=0,xp=0,selected_icon_id='default')).encode()
            elif self.path=='/v1/icons/catalog':
                body=b'[{"id":"default","name":"Default","price_bp":0,"default_owned":true}]'
            else: body=b'study-meta-ready'
            self.send_response(status)
            if location:self.send_header('Location',location)
            self.send_header('Content-Length',str(len(body)));self.end_headers();self.wfile.write(body)
        do_GET=respond;do_POST=respond
    class Server(ThreadingHTTPServer):
        daemon_threads=True
        def handle_error(self,*args):pass
        def get_request(self):
            sock,addr=super().get_request();sock.settimeout(2);return sock,addr
    http=Server(('127.0.0.1',0),Handler)
    if not plain:
        context=ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        context.load_cert_chain(str(folder/(name+'.pem')),str(folder/'server.key'))
        http.socket=context.wrap_socket(http.socket,server_side=True)
    thread=threading.Thread(target=http.serve_forever,kwargs={'poll_interval':0.02});thread.start()
    try:yield http.server_port,calls
    finally:http.shutdown();http.server_close();thread.join()

def matrix(executable, folder, *, root=False, before=False):
    env=os.environ.copy();env['TETRIS_CA_FILE']=str(folder/'ca.pem')
    def invoke(port,host='localhost',trusted=True, get_only=False):
        e=dict(env)
        if not trusted:e.pop('TETRIS_CA_FILE',None)
        args=[str(executable),f'https://{host}:{port}'] if root else [str(executable),host,str(port),str(folder/'ca.pem') if trusted else '-']
        if root and get_only:args.append("get")
        return subprocess.run(args,env=e,capture_output=True,text=True,timeout=8)
    cases=[('valid','localhost',True,0),('conflict','localhost',True,0 if before else 4),('cn-only','localhost',True,0 if before else 4),('ip','127.0.0.1',True,0),('valid','127.0.0.1',True,4),('valid','localhost',False,4),('expired','localhost',True,4)]
    for name,host,trusted,expected in cases:
        with server(folder,name) as(port,calls):
            result=invoke(port,host,trusted)
            assert result.returncode==expected,(name,host,trusted,result.returncode,result.stderr)
            assert bool(calls)==(expected==0),(name,calls)
            print(('before' if before else 'after'),executable.name,name,host,'trusted' if trusted else 'untrusted','exit',expected,'HTTP',len(calls),flush=True)
            if root and not before:
                previous=len(calls);get=invoke(port,host,trusted,get_only=True)
                assert get.returncode==expected and len(calls)-previous==(1 if expected==0 else 0),(name,get.returncode,calls)
                print('GET independently',name,'exit',expected,'new HTTP',len(calls)-previous,flush=True)
    with server(folder,plain=True) as(port,calls):
        result=invoke(port);assert result.returncode==4 and not calls,(result.returncode,calls)
        print(executable.name,'HTTPS-to-plain rejected; no HTTP fallback',flush=True)
    with server(folder,plain=True) as(target,hits):
        with server(folder,status=302,location=f'http://127.0.0.1:{target}/stolen') as(port,calls):
            result=invoke(port);assert result.returncode in (4,5) and len(calls)==1 and not hits,(result.returncode,calls,hits)
            print(executable.name,'redirect not followed; target HTTP=0',flush=True)
