"""Refresh the online/offline atlas UI using committed exports, without a ROM."""
from pathlib import Path
import hashlib,json,zipfile
from build_online_atlas import write_online_atlas
ROOT=Path(__file__).resolve().parents[1]
def correct_health(text):
    return text.replace('100 health (200 cap)','100 health (collect below 200)').replace('H100 can raise health to 200.','H100 adds 100 when health is below 200; it does not cap the result.').replace('H100 adds 100 health with a 200 cap.','H100 adds 100 health when below 200, without capping the result.')
def viewer(previous):
    start=previous.index('const atlas=')+len('const atlas=')
    end=previous.index(';\nconst $=',start)
    data=correct_health(previous[start:end])
    json.loads(data)
    script=(ROOT/'scripts/topmap_viewer.js').read_text(encoding='utf8').replace('__DATA__',data)
    return correct_health((ROOT/'scripts/topmap_viewer.html').read_text(encoding='utf8').replace('__STYLE__',(ROOT/'scripts/topmap_viewer.css').read_text(encoding='utf8')).replace('__SCRIPT__',script))
def main():
    atlas=ROOT/'site/atlas';archive=ROOT/'site/downloads/ssa-topmaps.zip'
    # Keep the ZIP's standalone interface and the website's iframe integration.
    with zipfile.ZipFile(archive) as z:
        contents={item.filename:(item,z.read(item)) for item in z.infolist()}
    for name,(info,data) in list(contents.items()):
        if name.endswith(('.html','.json','.svg','.txt','.md')):
            text=data.decode('utf8')
            text=viewer(text) if name=='index.html' else correct_health(text)
            contents[name]=(info,text.encode('utf8'))
    hashes={name:hashlib.sha256(data).hexdigest() for name,(_,data) in contents.items() if name!='manifest-sha256.json'}
    info,_=contents['manifest-sha256.json'];contents['manifest-sha256.json']=(info,(json.dumps(hashes,indent=2)+'\n').encode())
    with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
        for name,(info,data) in contents.items():z.writestr(info,data)
    for path in atlas.rglob('*'):
        if path.is_file() and path.suffix in ('.html','.json','.svg','.txt','.md'):
            if path.name=='index.html':continue
            text=path.read_text(encoding='utf8')
            path.write_text(correct_health(text),encoding='utf8')
    write_online_atlas(atlas,contents['index.html'][1].decode('utf8'))
    hashes={p.relative_to(atlas).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(atlas.rglob('*')) if p.is_file() and p.name!='manifest-sha256.json'}
    (atlas/'manifest-sha256.json').write_text(json.dumps(hashes,indent=2)+'\n')
    for path in (ROOT/'site/data').glob('level-*.json'):
        text=path.read_text(encoding='utf8');path.write_text(correct_health(text),encoding='utf8')
    print('Updated online atlas and offline archive, preserving exported geometry and images.')
if __name__=='__main__':main()
