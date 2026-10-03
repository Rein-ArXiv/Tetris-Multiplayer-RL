"""Export a self-contained static learning site, never the repository/server root.

Outputs content-addressed releases and reproducible ZIPs under out/learning-site.
No network, deployment, credentials, or user answers are part of this operation.
"""
from __future__ import annotations
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[1]
LEARN = ROOT/'docs/learn'
OUT = ROOT/'out/learning-site'
ASSETS = (
    'index.html', 'style.css', 'site-config.js', 'library.js', 'lessons.js',
    'course.js', 'assets.js', 'app.js', 'usability.js', 'reader.js', 'vendor/mermaid.min.js',
    'vendor/highlight.min.js', 'vendor/mermaid-LICENSE', 'vendor/highlight-LICENSE',
    'vendor/README.md',
)
EXPORT_VERSION = 1

def digest(raw: bytes) -> str:
    return hashlib.sha256(raw).hexdigest()

def prepare() -> tuple[str, dict[str, bytes]]:
    for script in ['build_learning_library.py','build_learning_coverage.py','build_learning_lessons.py']:
        subprocess.run([sys.executable,str(ROOT/'scripts'/script),'--check'],check=True,cwd=ROOT)
    files = {}
    for name in ASSETS:
        path = LEARN/name
        if path.is_symlink() or not path.resolve().is_relative_to(LEARN.resolve()):
            raise ValueError(f'Asset must be a regular local file: {name}')
        files[name] = path.read_bytes()
    seed = json.dumps({'exportVersion':EXPORT_VERSION,'assets':{p:digest(b) for p,b in files.items()}},sort_keys=True).encode()
    release = digest(seed)[:16]
    files['site-config.js'] = ('window.LEARNING_SITE = Object.freeze('+json.dumps({'mode':'static','release':release,'assets':{name:digest(files[name])[:16] for name in ('library.js','vendor/mermaid.min.js')}})+');\n').encode()
    html = files['index.html'].decode()
    # All executable/style assets are relative and versioned by their exact bytes.
    def version(match):
        name=match[2].split('?',1)[0]
        if name not in files:
            raise ValueError(f'Entry point depends on an unbundled asset: {name}')
        return f'{match[1]}="{name}?v={digest(files[name])[:16]}"'
    html = re.sub(r'(src|href)="([^"#]+\.(?:js|css)(?:\?[^" ]*)?)"',version,html)
    files['index.html'] = html.encode()
    manifest = {'format':'tetris-learning-static-site','version':1,'release':release,
                'sourceMode':'bundled-snapshot','progressMode':'browser-local',
                'files':{p:{'bytes':len(b),'sha256':digest(b)} for p,b in sorted(files.items())}}
    files['site-manifest.json']=(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n').encode()
    return release,files

def main():
    release,files=prepare()
    directory=OUT/'releases'/release
    archive=OUT/(release+'.zip')
    # An existing release is immutable. Never delete or overwrite unrelated files.
    if directory.exists():
        actual={p.relative_to(directory).as_posix():p.read_bytes() for p in directory.rglob('*') if p.is_file()}
        if actual != files: raise ValueError(f'Existing release differs: {directory}')
    else:
        directory.mkdir(parents=True)
        for name,raw in files.items():
            dest=directory/name;dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(raw)
    temporary=archive.with_suffix('.zip.tmp')
    with zipfile.ZipFile(temporary,'w',compression=zipfile.ZIP_DEFLATED) as z:
        for name,raw in sorted(files.items()):
            info=zipfile.ZipInfo(name,date_time=(1980,1,1,0,0,0))
            info.compress_type=zipfile.ZIP_DEFLATED
            info.external_attr=0o100644 << 16
            z.writestr(info,raw)
    temporary.replace(archive)
    print(json.dumps({'release':release,'directory':str(directory),'zip':str(archive),'files':len(files)},ensure_ascii=False))

if __name__=='__main__':main()
