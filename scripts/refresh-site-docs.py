"""Refresh published references without re-exporting unchanged ROM assets."""
from pathlib import Path
import shutil
from build_project_site import document
from build_wiki import main as build_wiki

root = Path(__file__).resolve().parents[1]
for path in [root / 'README.md', *sorted((root / 'analysis').glob('*.md'))]:
    (root / 'site/docs' / f'{path.stem}.html').write_text(document(path), encoding='utf8')
    shutil.copy2(path, root / 'site/docs' / path.name)
build_wiki()
print('Published references refreshed.')
