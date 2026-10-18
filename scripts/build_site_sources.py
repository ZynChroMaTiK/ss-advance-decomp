"""Publish the existing secret-source records as a readable page."""
from pathlib import Path
import html, json

ROOT = Path(__file__).resolve().parents[1]
def main():
    records = json.loads((ROOT/'site/atlas/sources-and-secret-coverage.json').read_text(encoding='utf8'))
    esc = html.escape
    cards = []
    for level in records:
        if not level['secrets']: continue
        links = ''.join(f'<li><strong>{esc(secret["title"])}</strong> — <a href="{esc(secret["source"],quote=True)}" target="_blank" rel="noopener">Read the source guide ↗</a></li>' for secret in level['secrets'])
        cards.append(f'<section><h2>Level {level["level"]}: {esc(level["title"])}</h2><ul>{links}</ul><p><a href="index.html#levels/{level["level"]}/2d">Find it on the map →</a></p></section>')
    page = '''<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Secret sources — Serious Sam Advance</title><link rel="stylesheet" href="styles.css"><link rel="stylesheet" href="reference.css"><link rel="stylesheet" href="accessibility.css"></head><body>
<a class="skip" href="#main">Skip to content</a><header class="site-header"><a class="brand" href="index.html#project">Serious Sam Advance</a><a class="text-link" href="index.html#levels/1/2d">← Back to maps</a></header>
<main id="main" class="reference" tabindex="-1"><h1>Where the secret tips came from</h1><p>The map browser marks five known secrets in levels 1–5. The guides below identify the hidden pickups; we matched them to their positions in the game’s maps.</p><p>This is a partial list. A level with no markers may still have secrets to find.</p>'''+''.join(cards)+'''<p>The linked guides are maintained by other people and may contain spoilers.</p><p><a href="atlas/sources-and-secret-coverage.json">Download the source records (JSON)</a> · <a href="index.html#development">Download and help</a></p></main></body></html>'''
    (ROOT/'site/secret-sources.html').write_text(page+'\n',encoding='utf8')
    print('Published readable secret sources:',len(cards),'levels')
if __name__ == '__main__': main()
