"""Assemble the static GitHub Pages site from verified local exports.

No ROM is copied. Run export_topmaps.py and
export_site_levels.py first when the recovered inputs change.
"""
from pathlib import Path
import json,shutil
ROOT=Path(__file__).resolve().parents[1]
SITE=ROOT/'site'
ATLAS=ROOT/'build/exports/ssa-topmaps'

def document(path):
    from site_documents import render_document
    available={'README.md','DEVELOPMENT.md'}|{'analysis/'+p.name for p in (ROOT/'analysis').glob('*.md')}
    return render_document(path.read_text(encoding='utf8'),path.relative_to(ROOT).as_posix(),available)


def main():
    shutil.copytree(ATLAS/'icons',SITE/'assets/map-icons',dirs_exist_ok=True)
    shutil.copytree(ATLAS/'theme',SITE/'assets/theme',dirs_exist_ok=True)
    shutil.copy2(ATLAS/'sources-and-secret-coverage.json',SITE/'data/sources-and-secret-coverage.json')
    manifest=json.loads((SITE/'assets/textures/manifest.json').read_text())
    (SITE/'data/export-validation.json').write_text(json.dumps(manifest['validation'],indent=2)+'\n')
    (SITE/'docs').mkdir(exist_ok=True)
    for path in [ROOT/'README.md',ROOT/'DEVELOPMENT.md',*sorted((ROOT/'analysis').glob('*.md'))]:
        (SITE/'docs'/f'{path.stem}.html').write_text(document(path),encoding='utf8')
        shutil.copy2(path,SITE/'docs'/path.name)
    from build_wiki import main as build_wiki
    build_wiki()
    from build_site_graphics import main as build_graphics
    build_graphics()
    from build_site_sources import main as build_sources
    build_sources()
    (SITE/'.nojekyll').touch()
    print('Site assembled:',len(list(SITE.rglob('*'))),'paths')

if __name__=='__main__':main()
