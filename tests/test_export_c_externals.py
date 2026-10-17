"""Exercise C-only canonical references using real ARM ELF relocations."""
from pathlib import Path
import ast,json,re,subprocess,tempfile
from elftools.elf.elffile import ELFFile
ROOT=Path(__file__).resolve().parents[1]
source=ROOT/'tools/decomp-wtf/src/decompnet/rom/export_gba_build.py'
assembler=ROOT/'.toolchains/arm-gnu/bin/arm-none-eabi-as'
linker=ROOT/'.toolchains/arm-gnu/bin/arm-none-eabi-ld'
functions=[n for n in ast.parse(source.read_text()).body if isinstance(n,ast.FunctionDef) and n.name in ('_globals','_externals','linker_script')]
with tempfile.TemporaryDirectory(prefix='ssa-export-') as temporary:
 root=Path(temporary);(root/'build/src').mkdir(parents=True);(root/'asm').mkdir()
 relative='src/decompnet/rom/export_gba_build.py'
 pristine=subprocess.run(['git','show','HEAD:'+relative],cwd=ROOT/'tools/decomp-wtf',check=True,capture_output=True).stdout
 patched=root/relative;patched.parent.mkdir(parents=True);patched.write_bytes(pristine.replace(b'\r\n',b'\n'))
 for patch in ('decomp-wtf-thumb-symbols.patch','decomp-wtf-c-externals.patch'):
  subprocess.run(['git','apply',str(ROOT/'patches'/patch)],cwd=root,check=True,capture_output=True)
 assert patched.read_text()==source.read_text()
 subprocess.run(['git','apply','--reverse','--check',str(ROOT/'patches/decomp-wtf-c-externals.patch')],cwd=root,check=True,capture_output=True)
 (root/'asm/pieces.json').write_text('[]')
 assembly='''.syntax unified
.thumb
.text
.global recovered
.thumb_func
recovered:
bl sub_0800036C
ldr r0, literal
bx lr
.align 2
literal: .word D_030000F0
.arm
.global arm_caller
arm_caller:
bl sub_08000400
bx lr
'''
 (root/'build/src/recovered.s').write_text(assembly)
 obj='build/src/recovered.o'
 subprocess.run([str(assembler),'-mcpu=arm7tdmi','-o',str(root/obj),str(root/'build/src/recovered.s')],check=True)
 with open(root/obj,'rb') as f:original_size=ELFFile(f).get_section_by_name('.text')['sh_size']
 ns={'ROOT':root,'B':{'base':0x08010000,'functions':[],'symbols':{}},'json':json,'re':re,'Path':Path,'AS_FLAGS':['-mcpu=arm7tdmi'],'_LABEL':re.compile(r'\b(_[0-9A-F]{8})\b')}
 ns['arm']=lambda tool:str(assembler if tool=='as' else linker)
 ns['run']=lambda args:subprocess.run(args,cwd=root,check=True,capture_output=True)
 exec(compile(ast.Module(functions,type_ignores=[]),'export helpers','exec'),ns)
 wanted,thumb=ns['_externals'](obj)
 assert wanted=={'sub_0800036C','sub_08000400','D_030000F0'}
 assert thumb=={'sub_0800036C'}
 script=ns['linker_script']([(0x08010000,obj,'.text')],{'recovered':obj})
 assert 'D_030000F0 = 0x030000F0;' in script
 assert 'sub_08000400 = 0x08000400;' in script
 with open(root/'build/thumb_symbols.o','rb') as f:
  syms=ELFFile(f).get_section_by_name('.symtab')
  s=syms.get_symbol_by_name('sub_0800036C')[0]
  assert s['st_info']['type']=='STT_FUNC' and s['st_value']==0x0800036d
  assert syms.get_symbol_by_name('sub_08000400') is None
 (root/'build/game.ld').write_text(script)
 subprocess.run([str(linker),'-T','build/game.ld','-o','build/game.elf',obj,'build/thumb_symbols.o'],cwd=root,check=True,capture_output=True)
 with open(root/'build/game.elf','rb') as f:
  elf=ELFFile(f);syms=elf.get_section_by_name('.symtab')
  assert syms.get_symbol_by_name('D_030000F0')[0]['st_value']==0x030000f0
  assert elf.get_section_by_name('.rom')['sh_size']==original_size # no generated interworking veneer
print('PASS both patches apply to pinned upstream; C-only Thumb thunk/data references and ARM-call mode; real ELF link adds no veneer')
