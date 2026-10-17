from pathlib import Path
import hashlib,json,os,subprocess,sys
r=Path(__file__).resolve().parents[1];b=r/'build/native-tests';b.mkdir(parents=True,exist_ok=True)
sources=sorted((r/'src').glob('*.c'))
music=subprocess.run([sys.executable,str(r/'tests/test_music_selection.py')],capture_output=True,text=True,check=True)
music_report=json.loads(music.stdout)
echo=subprocess.run([sys.executable,str(r/'tests/test_audio_echo.py')],capture_output=True,text=True,check=True)
echo_report=json.loads(echo.stdout)
policy=subprocess.run([sys.executable,str(r/'tests/test_audio_policy.py')],capture_output=True,text=True,check=True)
policy_report=json.loads(policy.stdout)
channels=subprocess.run([sys.executable,str(r/'tests/test_audio_channels.py')],capture_output=True,text=True,check=True)
channels_report=json.loads(channels.stdout)
wave=subprocess.run([sys.executable,str(r/'tests/test_audio_wave.py')],capture_output=True,text=True,check=True)
wave_report=json.loads(wave.stdout)
subprocess.run(['gcc','-std=c11','-O2','-g','-Wall','-Wextra','-Werror','-pedantic',
 '-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie','-I',str(r/'include'),
 str(r/'tests/test_audio.c'),*map(str,sources),'-o',str(b/'test_audio')],check=True)
env=os.environ.copy();env['ASAN_OPTIONS']='detect_leaks=1'
p=subprocess.run([str(b/'test_audio'),str(r/'orig/SSA.gba')],capture_output=True,text=True,env=env,timeout=300)
print(p.stdout,end='');print(p.stderr,end='');p.check_returncode()
report=json.loads(p.stdout);report['sanitizers']='ASan/UBSan/leaks=1'
report['music_selection_rom_comparisons']=music_report['rom_comparisons']
report['pcm_envelope_rom_comparisons']=echo_report['pcm_envelope_rom_comparisons']
report['echo_command_rom_comparisons']=echo_report['echo_command_rom_comparisons']
report['original_reference_arbitration_rom_comparisons']=channels_report['original_reference_arbitration_rom_comparisons']
report['volume_rom_comparisons']=channels_report['volume_rom_comparisons']
report['wave_note_rom_comparisons']=wave_report['wave_note_rom_comparisons']
report['pitch_register_rom_comparisons']=wave_report['pitch_register_rom_comparisons']
report['wave_dac_level_checks']=wave_report['dac_level_checks']
report['music_policy_rom_comparisons']=sum(policy_report[k] for k in ('fade_frame_rom_comparisons','request_rom_comparisons','fade_start_rom_comparisons','automatic_return_rom_comparisons'))
inputs=sources+list((r/'include/ssa').glob('*.h'))+[r/'src/native_audio.inc',r/'tests/test_audio.c',r/'tests/audio_concurrency.inc',r/'tests/audio_world.inc',r/'tests/audio_priority.inc',Path(__file__)]
report['source_sha256']={str(s.relative_to(r)):hashlib.sha256(s.read_bytes()).hexdigest() for s in inputs}
(r/'analysis/audio-validation.json').write_text(json.dumps(report,indent=2)+'\n')
