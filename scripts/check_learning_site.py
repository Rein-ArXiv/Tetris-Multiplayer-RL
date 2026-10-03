"""Validate the static export and its reproducibility without deploying it."""
from __future__ import annotations
import hashlib
from html.parser import HTMLParser
import json
from pathlib import Path
import subprocess
import sys
from urllib.parse import urlsplit
import zipfile
from build_learning_site import ASSETS, ROOT

class EntryAssets(HTMLParser):
    def __init__(self):super().__init__();self.paths=[]
    def handle_starttag(self,tag,attrs):
        attrs=dict(attrs)
        if tag=='script' and 'src' in attrs:self.paths.append(attrs['src'])
        if tag=='link' and attrs.get('rel')=='stylesheet':self.paths.append(attrs['href'])

def export():
    result=subprocess.run([sys.executable,str(ROOT/'scripts/build_learning_site.py')],
                          check=True,text=True,capture_output=True,cwd=ROOT)
    return json.loads(result.stdout.splitlines()[-1])

def main():
    first=export();directory=Path(first['directory']);archive=Path(first['zip'])
    before=hashlib.sha256(archive.read_bytes()).hexdigest()
    second=export()
    assert first==second and hashlib.sha256(archive.read_bytes()).hexdigest()==before
    names={p.relative_to(directory).as_posix() for p in directory.rglob('*') if p.is_file()}
    assert names==set(ASSETS)|{'site-manifest.json'}
    manifest=json.loads((directory/'site-manifest.json').read_text())
    for name,entry in manifest['files'].items():
        data=(directory/name).read_bytes()
        assert len(data)==entry['bytes'] and hashlib.sha256(data).hexdigest()==entry['sha256']
    parser=EntryAssets();parser.feed((directory/'index.html').read_text())
    for path in parser.paths:
        parsed=urlsplit(path)
        assert not parsed.scheme and not parsed.netloc and not parsed.path.startswith('/')
        assert parsed.path in names and parsed.query.startswith('v=')
    config=(directory/'site-config.js').read_text()
    assert '"mode": "static"' in config and 'sourceEndpoint' not in config
    with zipfile.ZipFile(archive) as z:
        assert set(z.namelist())==names
        assert all(z.read(name)==(directory/name).read_bytes() for name in names)
    print('Static export: allowlist, relative assets, hashes and reproducible ZIP passed')
    print(json.dumps(first))

if __name__=='__main__':main()
