"""Render repository references with Markdown semantics and resolved source links."""
from pathlib import PurePosixPath
from urllib.parse import urlsplit, unquote, quote
import html,re,posixpath
from markdown_it import MarkdownIt

REPO='https://github.com/ZynChroMaTiK/ss-advance-decomp/blob/main/'

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
    return f'''<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>{html.escape(title)} — Advance Project</title><link rel="stylesheet" href="../styles.css"><link rel="stylesheet" href="../reference.css"></head><body><a class="skip" href="#reference">Skip to reference</a><header class="site-header"><a href="../#development">← Build &amp; Source</a><a href="../#project">Project home</a><a href="{PurePosixPath(source_path).stem}.md" download>Markdown ↓</a></header><main id="reference" class="reference" tabindex="-1"><p class="reference-note">Project reference snapshot. Recovery notes retain earlier subsystem milestones; use the <a href="../#features">current feature overview</a> for playable scope.</p>{body}</main></body></html>'''
