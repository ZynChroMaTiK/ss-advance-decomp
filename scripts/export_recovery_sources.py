"""Publish authored recovery work with an explicit database-field allowlist.

No ROM/assembly/compiled blobs or server/authentication metadata are exported.
This is an archival source snapshot, not an importable live decomp database.
"""
from pathlib import Path
import hashlib, json, shutil, sqlite3

ROOT = Path(__file__).resolve().parents[1]

def main():
    output = ROOT / 'recovered/snapshot'
    output.mkdir(parents=True, exist_ok=True)
    db = sqlite3.connect((ROOT/'decomp/.decomp/decomp.db').as_uri()+'?mode=ro', uri=True)
    db.row_factory = sqlite3.Row
    slug = json.loads((ROOT/'decomp/project.json').read_text())
    # The workspace can retain older projects; only archive the active one.
    project_slug = slug.get('slug', slug.get('project', 'serious-sam-3'))
    if isinstance(project_slug, dict): project_slug = project_slug['slug']
    project = db.execute('SELECT id,slug FROM projects WHERE slug=?', (project_slug,)).fetchone()
    if project is None: raise RuntimeError('Active project not found: '+str(project_slug))
    pid = project['id']
    fields = 'name,size,n_insns,status,best_score,best_source,matched_source,attempts,unit,vram,rom_offset'
    functions = [dict(r) for r in db.execute('SELECT '+fields+' FROM functions WHERE project_id=? ORDER BY name',(pid,))]
    attempts = [dict(r) for r in db.execute('SELECT f.name AS function,a.score,a.compiled,a.context_version,a.source FROM attempts a JOIN functions f ON f.id=a.function_id WHERE f.project_id=? ORDER BY a.id',(pid,))]
    notes = [dict(r) for r in db.execute('SELECT f.name AS function,n.text FROM notes n LEFT JOIN functions f ON f.id=n.function_id WHERE n.project_id=? ORDER BY n.id',(pid,))]
    contexts = [dict(r) for r in db.execute('SELECT version,header,message,unit FROM context_versions WHERE project_id=? ORDER BY version,unit',(pid,))]
    payload = {'project':project['slug'],'scope':'Source and research only; catalogue discovery is incomplete. Best candidates are not necessarily exact matches.','functions':functions,'attempts':attempts,'notes':notes,'contexts':contexts}
    (output/'research.json').write_text(json.dumps(payload,indent=2)+'\n',encoding='utf8')
    exported = ROOT/'decomp'/project['slug']/'source'
    for folder in ('src','include'):
        for source in sorted((exported/folder).rglob('*')):
            if source.suffix not in ('.c','.h'): continue
            target=output/'exact'/folder/source.relative_to(exported/folder)
            target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source,target)
    for name in ('build.py','build.json','functions.cfg'):
        shutil.copy2(exported/name,output/'exact'/name)
    counts={'functions':len(functions),'matched_sources':sum(bool(f['matched_source']) for f in functions),'best_sources':sum(bool(f['best_source']) for f in functions),'attempts':len(attempts),'notes':len(notes),'contexts':len(contexts)}
    manifest={'counts':counts,'sha256':{str(p.relative_to(output)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(output.rglob('*')) if p.is_file() and p.name not in ('manifest.json','README.md')}}
    (output/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf8')
    print(json.dumps(counts))

if __name__ == '__main__': main()
