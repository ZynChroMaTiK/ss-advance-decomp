"""Whole A444 hitscan geometry with original A240/9DE4 execution on all maps.
Only terminal impact9D4C and damage37F4 calls are captured (no gameplay effects).
Their ordered arguments are compared with deferred native hit results, including
zero damage/health occlusion. Native combat integration is tested separately.
"""
from pathlib import Path
import ast
ROOT=Path(__file__).resolve().parents[1]
# Reuse declaration/bootstrap code, not the aim test cases or its report.
bootstrap=(ROOT/'tests/test_weapon_aim_rom.py').read_text().split("report={",1)[0]
bootstrap=bootstrap.replace("sources=['weapon_aim.c'", "sources=['weapon_hit.c','weapon_aim.c'")
bootstrap=bootstrap.replace('libssa_weapon_aim.so','libssa_weapon_hit.so')
exec(compile(bootstrap,str(ROOT/'tests/test_weapon_aim_rom.py'),'exec'))
from unicorn.arm_const import UC_ARM_REG_R5, UC_ARM_REG_R9, UC_ARM_REG_R10
class WeaponHit(C.Structure):
    _fields_=[('target',EP)]+[(n,C.c_int32) for n in ('x','y','z')]+[('impact',C.c_uint)]
lib.ssa_weapon_hit_query.argtypes=[C.POINTER(Aim),EP,C.c_uint8]+[C.c_int32]*5+[C.POINTER(WeaponHit)]
PREDICATE=C.CFUNCTYPE(C.c_int,C.c_void_p,C.POINTER(Hit))
lib.ssa_weapon_aim_trace.argtypes=[C.POINTER(Aim)]+[C.c_int32]*4+[PREDICATE,C.c_void_p]
effects=[]; expected_impact=[]
def terminal(machine,address,size,user):
    if address==0x08009fec:
        expected_impact[:]=[machine.reg_read(UC_ARM_REG_R5)]+[signed(machine.reg_read(r)) for r in (UC_ARM_REG_R10,UC_ARM_REG_R9,UC_ARM_REG_R2)]
    elif address==0x08009d4c:
        args=[signed(machine.reg_read(r)) for r in REGS[:3]]+[machine.reg_read(REGS[3])]
        effects.append(('impact',*args))
        if not args[3]: expected_impact[:]=[0]+args[:3]
        machine.reg_write(UC_ARM_REG_PC,machine.reg_read(UC_ARM_REG_LR))
    else:
        effects.append(('damage',machine.reg_read(REGS[0]),signed(machine.reg_read(REGS[1]))))
        machine.reg_write(UC_ARM_REG_PC,machine.reg_read(UC_ARM_REG_LR))
for address in (0x08009fec,0x08009d4c,0x080037f4): uc.hook_add(UC_HOOK_CODE,terminal,begin=address,end=address)
def run_hit(sx,sy,z,range_,slope,damage):
    for r,v in zip(REGS,(ENTITIES,sx,sy,z)): uc.reg_write(r,v&0xffffffff)
    put(0x03007e00,'<IIII',0xface1234,range_&0xffffffff,slope&0xffffffff,damage&0xffffffff)
    uc.reg_write(UC_ARM_REG_SP,0x03007e00); uc.reg_write(UC_ARM_REG_LR,STOP|1)
    uc.emu_start(0x0800a445,STOP,count=500000)
    assert uc.reg_read(UC_ARM_REG_PC)==STOP
    return signed(uc.reg_read(UC_ARM_REG_R0))
rng=random.Random(0xa444)
report={'rom_sha1':hashlib.sha1(ROM).hexdigest(),'whole_hitscan':0,'maps':16,'entity_hits':0,'wall_impacts':0,'no_impact':0,'suppressed_effects':0,'effect_calls':0,'native_guards':0,'reentry_checks':0,'callback_failures':0,'maximum_intercepts':0,'scope':'Whole A444/A240/9DE4; only terminal impact9D4C and damage37F4 captured, native deferred geometry compared plus ordered effect/damage arguments. No combat effects executed in this test.'}
for level in range(16):
    m=Map(); w=World(); a=Aim(); entities=(Entity*9)()
    assert lib.ssa_map_assets_load(C.byref(m),ROM,len(ROM),level)==1
    assert lib.ssa_world_init(C.byref(w),C.byref(m),len(entities))==1
    assert lib.ssa_weapon_aim_init(C.byref(a),C.byref(w),C.byref(tables))==1
    data=bytearray(C.string_at(m.data,m.size)); original=bytearray(data)
    for off in range(0x1c,0x48,4): struct.pack_into('<I',original,off,BASE+struct.unpack_from('<I',original,off)[0])
    uc.mem_write(BASE,bytes(original)); uc.mem_write(SCRATCH,bytes(m.line_count*4))
    put(0x03004930,'<I',BASE); put(0x03004944,'<iihh',w.origin_x,w.origin_y,w.width,w.height); put(0x03004954,'<I',SCRATCH); put(0x03004640,'<I',CELLS)
    vertices=list(struct.iter_unpack('<hh',data[m.vertices:m.vertices+m.vertex_count*4]))
    two=[j for j in range(m.line_count) if struct.unpack_from('<H',data,m.lines+j*40+26)[0]&4]
    native_index={C.addressof(e):j for j,e in enumerate(entities)}
    def eptr(e): return ENTITIES+native_index[pointer(e)]*0x100 if e else 0
    for i in range(512):
        for e in entities:
            if e.owner: assert lib.ssa_world_remove(C.byref(w),C.byref(e))==1
        vx,vy=rng.choice(vertices)
        facing=rng.randrange(256) if i%3 else rng.choice((0,64,128,192))
        sine,cosine=C.c_int32(),C.c_int32(); lib.ssa_sincos(C.byref(tables),facing<<8,C.byref(sine),C.byref(cosine))
        x=vx*65536+rng.randrange(-64*65536,64*65536); y=vy*65536+rng.randrange(-64*65536,64*65536)
        if i%13==0: x=w.origin_x+rng.randrange(w.width)*128*65536
        if i%17==0: y=w.origin_y+rng.randrange(w.height)*128*65536
        z=rng.choice((-32,0,16,32,96))*65536
        health=[]
        for j,e in enumerate(entities):
            d=0 if j==0 else rng.randrange(-8,128)
            e.body.x=signed(x+sine.value*d); e.body.y=signed(y+cosine.value*d)
            if j>1 and i%4: e.body.x=signed(e.body.x+rng.randrange(-24,24)*65536)
            if i%11==0 and j==2: e.body.x=entities[1].body.x; e.body.y=entities[1].body.y
            e.body.z=z+rng.randrange(-32,32)*65536; e.body.height=rng.choice((0,8,32,64,96))*65536
            e.radius=rng.choice((0,1,8,16,32))*65536; e.type=rng.choice((0xcd,0xd7,0xe1,0x67))
            e.body.flags=rng.choice((0,2,2,2,0x22)); health.append(rng.choice((-1,0,0,1,100)))
            assert lib.ssa_world_insert(C.byref(w),C.byref(e))==1
        rawcells=bytearray(w.width*w.height*4)
        for cell in range(w.width*w.height): struct.pack_into('<I',rawcells,cell*4,eptr(w.cells[cell]))
        uc.mem_write(CELLS,bytes(rawcells))
        for j,e in enumerate(entities):
            raw=bytearray(0x74); struct.pack_into('<iii',raw,0,e.body.x,e.body.y,e.body.z)
            struct.pack_into('<I',raw,0x18,eptr(e.block_next)); struct.pack_into('<iiHh',raw,0x28,e.radius,e.body.height,e.body.flags,health[j]); struct.pack_into('<H',raw,0x3c,e.type); raw[0x3f]=facing
            uc.mem_write(ENTITIES+j*0x100,bytes(raw))
        if i>=256 and i%4==0:
            line=rng.choice(two); front,back=struct.unpack_from('<hh',data,m.lines+line*40+12)
            for sector in (front,back):
                raw=struct.pack('<hh',rng.choice((-32768,-32,0,16,32,96,32767)),rng.choice((-32768,0,32,96,128,32767)))
                off=m.sectors+sector*20; C.memmove(C.addressof(m.data.contents)+off,raw,4); uc.mem_write(BASE+off,raw)
        for field,addr in [('marker',0x03005074),('top_slope',0x03004764),('bottom_slope',0x03005070),('start_x',0x03005050),('start_y',0x03005054),('end_x',0x030054d0),('end_y',0x030054d4)]:
            value=signed(rng.getrandbits(32)) if i%4==0 else rng.randrange(-0xa000,0xa001)
            setattr(a,field,value); put(addr,'<i',value)
        a.target=C.pointer(entities[8]); put(0x0300491c,'<I',eptr(a.target))
        a.traversal.blocking_fraction=signed(rng.getrandbits(32)); put(0x0300505c,'<i',a.traversal.blocking_fraction)
        for (name,_),addr in zip(Opening._fields_,OPEN):
            value=signed(rng.getrandbits(32)); setattr(a.opening,name,value); put(addr,'<i',value)
        sx=rng.choice((0,1,4,-4))*65536+rng.randrange(65536); sy=rng.choice((0,1,4,-4))*65536+rng.randrange(65536)
        offset=rng.choice((-32,0,16,32,48,96))*65536; range_=rng.choice((0,1,16,32,64,128,256,512))*65536+rng.randrange(65536)
        slope=rng.choice((-2147483648,-65536,-0xa000,-1,0,0,0,1,0xa000,65536,2147483647)) if i%2 else rng.randrange(-0xa000,0xa001)
        damage=rng.choice((-1,0,0,1,17,32767)); hit=WeaponHit()
        w.blocking_entity=C.pointer(entities[8]); blocker=pointer(w.blocking_entity)
        original_hits.clear(); expected_impact.clear(); effects.clear(); run_hit(sx,sy,offset,range_,slope,damage)
        assert lib.ssa_weapon_hit_query(C.byref(a),C.byref(entities[0]),facing,sx,sy,offset,range_,slope,C.byref(hit))==1
        actual=[eptr(hit.target),hit.x,hit.y,hit.z] if hit.impact else []
        assert actual==expected_impact,(level,i,'impact',actual,expected_impact)
        native_effects=[]
        if hit.impact:
            if not hit.target or (damage and health[native_index[pointer(hit.target)]]):
                native_effects.append(('impact',hit.x,hit.y,hit.z,eptr(hit.target)))
                if hit.target: native_effects.append(('damage',eptr(hit.target),damage))
            else: report['suppressed_effects']+=1
        assert native_effects==effects,(level,i,'effect_order',native_effects,effects)
        for field,addr in [('marker',0x03005074),('top_slope',0x03004764),('bottom_slope',0x03005070),('shoot_z',0x03005048),('range',0x03004920),('start_x',0x03005050),('start_y',0x03005054),('end_x',0x030054d0),('end_y',0x030054d4)]:
            assert getattr(a,field)==get(addr,'<i'),(level,i,field,getattr(a,field),get(addr,'<i'))
        assert eptr(a.target)==get(0x0300491c,'<I')
        assert bytes(a.traversal.ray)==bytes(uc.mem_read(0x03004900,16))
        assert a.traversal.blocking_fraction==get(0x0300505c,'<i')
        assert [getattr(a.opening,n) for n,_ in Opening._fields_]==[get(addr,'<i') for addr in OPEN]
        assert a.traversal.count==len(original_hits)
        for j,(fraction,kind,p) in enumerate(original_hits):
            native=a.traversal.intercepts[j]
            expected_pointer=BASE+m.lines+native.line*40 if native.line<m.line_count else eptr(a.entities[native.line-m.line_count])
            assert native.fraction==get(BUFFER+j*12,'<i') and expected_pointer==p,(level,i,j)
        assert pointer(w.blocking_entity)==blocker and not w.query_active and not a.updating
        report['whole_hitscan']+=1; report['entity_hits' if hit.target else 'wall_impacts' if hit.impact else 'no_impact']+=1
        report['effect_calls']+=len(effects); report['maximum_intercepts']=max(report['maximum_intercepts'],a.traversal.count)
    for obj,field in ((a,'updating'),(a.traversal,'updating'),(a.walls,'updating'),(w,'query_active')):
        setattr(obj,field,1); assert lib.ssa_weapon_hit_query(C.byref(a),C.byref(entities[0]),0,0,0,0,0,0,C.byref(hit))==-1
        setattr(obj,field,0); report['native_guards']+=1
    for e in entities:
        if e.owner: assert lib.ssa_world_remove(C.byref(w),C.byref(e))==1
    e=entities[0]; e.body.x=w.origin_x+64*65536; e.body.y=w.origin_y+64*65536; e.radius=16*65536
    assert lib.ssa_world_insert(C.byref(w),C.byref(e))==1
    callback_seen=[]
    @PREDICATE
    def reject_reentry(context,record):
        result=C.c_int32()
        checks=[w.query_active==1,a.updating==1,
            lib.ssa_weapon_aim_query(C.byref(a),C.byref(e),0,0,0,0,0,C.byref(result))==-1,
            lib.ssa_weapon_hit_query(C.byref(a),C.byref(e),0,0,0,0,0,0,C.byref(hit))==-1,
            lib.ssa_weapon_aim_trace(C.byref(a),0,0,0,0,reject_reentry,None)==-1,
            lib.ssa_weapon_aim_free(C.byref(a))==-1,
            lib.ssa_world_remove(C.byref(w),C.byref(e))==0]
        callback_seen.append(checks)
        return -1
    assert lib.ssa_weapon_aim_trace(C.byref(a),e.body.x-32*65536,e.body.y,e.body.x+32*65536,e.body.y,reject_reentry,None)==-1
    assert callback_seen and all(all(checks) for checks in callback_seen)
    assert not a.updating and not a.traversal.updating and not a.walls.updating and not w.query_active
    report['reentry_checks']+=5*len(callback_seen); report['callback_failures']+=1
    assert lib.ssa_weapon_aim_free(C.byref(a))==1; lib.ssa_world_free(C.byref(w)); lib.ssa_map_assets_free(C.byref(m))
    print(f'PASS map {level}: 512 whole A444 hitscan queries',flush=True)
report['source_sha256']={f:hashlib.sha256((ROOT/'src'/f).read_bytes()).hexdigest() for f in sources}
(ROOT/'analysis/native-weapon-hit-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
