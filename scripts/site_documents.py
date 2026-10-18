"""Render repository references with Markdown semantics and resolved source links."""
from pathlib import PurePosixPath
from urllib.parse import urlsplit, unquote, quote
import html,re,posixpath
from markdown_it import MarkdownIt

REPO='https://github.com/ZynChroMaTiK/ss-advance-decomp/blob/main/'

def group_history(body, source_path):
    ranges={
        'DEVELOPMENT.md':[('verified-progress','use-the-existing-workspace-from-powershell','Subsystem evidence and recovery history')],
        'analysis/porting.md':[
            ('next-work-required-for-the-requested-playable-port','playable-level-one-checkpoint-2026-10-15','Earlier recovery plan and subsystem checkpoints'),
            ('playable-level-one-checkpoint-2026-10-15','native-weapon-corrections-2026-10-16','Historical checkpoint: first playable level — 15 October 2026'),
            ('native-weapon-corrections-2026-10-16',None,'Historical checkpoints: weapon corrections and campaign progression — 16 October 2026')]
    }.get(source_path,[])
    changed=False
    # Reverse order keeps each boundary outside previously wrapped sections.
    for start,end,label in reversed(ranges):
        marker=f'<h2 id="{start}">';boundary=f'<h2 id="{end}">' if end else None
        if marker not in body:continue
        first=body.index(marker);last=body.index(boundary,first) if boundary and boundary in body else len(body)
        # If the next section was already grouped, close before its details wrapper.
        if boundary:
            wrapper=body.rfind('<details class="recovery-history">',first,last)
            if wrapper>=first:last=wrapper
        note='<p>These are older development notes. See <a href="README.html#current-prototype">What you can play</a> and <a href="native-game.html">Playable build details</a> for what you can play today.</p>'
        body=body[:first]+f'<details class="recovery-history"><summary>{html.escape(label)}</summary>'+note+body[first:last]+'</details>'+body[last:]
        changed=True
    return body,changed

def render_document(text, source_path, available):
    parser=MarkdownIt('commonmark',{'html':False}).enable('table')
    tokens=parser.parse(text);seen={};title=PurePosixPath(source_path).stem.replace('-',' ').title()
    if not any(t.type=='heading_open' and t.tag=='h1' for t in tokens):tokens=parser.parse('# '+title+'\n\n')+tokens
    has_title=False;shift=0;heading_tag='h1'
    for i,token in enumerate(tokens):
        if token.type=='heading_open':
            label=''.join(t.content for t in tokens[i+1].children or [] if t.type in ('text','code_inline'))
            if token.tag=='h1':
                if has_title:shift=1
                else:title=label;has_title=True
            heading_tag='h'+str(min(6,int(token.tag[1])+shift));token.tag=heading_tag
            slug=re.sub(r'[^\w\- ]','',label.lower()).replace(' ','-')
            if source_path=='README.md':
                slug={'what-you-can-play':'current-prototype','settings-and-extras':'make-it-yours','saves-and-problems':'progress-and-troubleshooting','building-it-yourself':'build-from-a-fresh-checkout'}.get(slug,slug)
            duplicate=seen.get(slug,0);seen[slug]=duplicate+1
            token.attrSet('id',slug+(f'-{duplicate}' if duplicate else ''))
        elif token.type=='heading_close':token.tag=heading_tag
        for child in token.children or []:
            if child.type!='link_open':continue
            href=child.attrGet('href');url=urlsplit(href)
            if url.scheme or url.netloc or not url.path:continue
            target=posixpath.normpath(posixpath.join(str(PurePosixPath(source_path).parent),unquote(url.path)))
            if target in available:
                href=PurePosixPath(target).stem+'.html'+('?' + url.query if url.query else '')+('#'+url.fragment if url.fragment else '')
            else:href=REPO+quote(target,safe='/')+('#'+url.fragment if url.fragment else '')
            child.attrSet('href',href)
    body=parser.renderer.render(tokens,parser.options,{})
    body,history=group_history(body,source_path)
    if history:body+='<script type="module" src="../reference.js"></script>'
    return f'''<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>{html.escape(title)} — Advance Project</title><link rel="stylesheet" href="../styles.css"><link rel="stylesheet" href="../reference.css"></head><body><a class="skip" href="#reference">Skip to reference</a><header class="site-header"><a href="../#development">← Download &amp; help</a><a href="../#project">Project home</a><a href="{PurePosixPath(source_path).stem}.md" download>Download text ↓</a></header><main id="reference" class="reference" tabindex="-1"><p class="reference-note">Need help getting started? Try the <a href="README.html">player guide</a>. These notes include older work; the <a href="../#features">game overview</a> explains what’s ready now.</p>{body}</main></body></html>'''
