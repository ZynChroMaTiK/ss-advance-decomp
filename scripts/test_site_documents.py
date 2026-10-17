from site_documents import render_document
sample='''# Reference `guide`

| Routine | State |
| --- | --- |
| one | recovered |

1. Build
2. Play

## Some section

[Local](movement.md#established-behavior) [Source](../recovered/snapshot/README.md)

## Some section
'''
page=render_document(sample,'analysis/guide.md',{'analysis/movement.md'})
for value in ['<h1 id="reference-guide">','<table>','<th>Routine</th>','<ol>','<li>Build</li>','href="movement.html#established-behavior"','id="some-section-1"','blob/main/recovered/snapshot/README.md']:
    assert value in page,value
assert '<script>' not in render_document('<script>alert(1)</script>','README.md',set())
assert render_document('# One\n\n# Two\n\n## Detail','README.md',set()).count('<h1 ')==1
from pathlib import Path
root=Path(__file__).resolve().parents[1]
for name,count in [('README',1),('porting',3)]:
    source='README.md' if name=='README' else 'analysis/porting.md'
    actual=render_document((root/f'site/docs/{name}.md').read_text(),source,set())
    assert actual.count('<details class="recovery-history">')==count
    assert actual.count('</details>')==count
    assert '<details class="recovery-history" open' not in actual
    assert 'src="../reference.js"' in actual
    assert actual.count('<h1 ')==1
    if name=='README':
        assert actual.index('<details')<actual.index('id="verified-progress"')<actual.index('</details>')<actual.index('id="use-the-existing-workspace-from-powershell"')
print('PASS: Markdown tables, lists, heading anchors, duplicate headings, local fragments, repository-relative links and escaped HTML')
