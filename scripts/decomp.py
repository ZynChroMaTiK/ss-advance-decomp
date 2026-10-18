"""Workspace-local driver for the pinned decomp.wtf checkout (run under WSL)."""
from pathlib import Path
import argparse
import json
import os
import time

ROOT = Path(__file__).resolve().parents[1]
os.environ.setdefault("DECOMPNET_TOOLCHAINS", str(ROOT / ".toolchains"))
os.environ.setdefault("DECOMP_HOME", str(ROOT / "decomp"))
os.environ.setdefault("UV_CACHE_DIR", str(ROOT / ".cache" / "uv"))

TESTS = {
    "test-sector-clip": "test_sector_clip_rom.py", "test-sector-clip-runtime": "test_sector_clip_runtime.py",
    "test-input": "test_input_rom.py", "test-movement": "test_movement_rom.py",
    "test-physics": "test_physics_rom.py", "test-maps": "test_map_assets.py",
    "test-geometry": "test_geometry_rom.py", "test-view": "test_view.py",
    "test-world": "test_world_rom.py", "test-slide": "test_slide_rom.py",
    "test-horizontal": "test_horizontal_rom.py", "test-inventory": "test_inventory_rom.py",
    "test-pickup": "test_pickup_rom.py", "test-spawn": "test_spawn_rom.py",
    "test-weapon-select": "test_weapon_select_rom.py", "test-entity-timer": "test_entity_timer_rom.py",
    "test-player-spawn": "test_player_spawn_rom.py", "test-player-motion": "test_player_motion_rom.py",
    "test-player-frame": "test_player_frame_rom.py", "test-player-runtime": "test_player_runtime.py",
    "test-respawn-select": "test_respawn_select_rom.py",
    "test-player-lifecycle": "test_player_lifecycle_rom.py",
    "test-player-hazards": "test_player_hazards_rom.py",
    "test-player-gameplay": "test_player_gameplay.py",
    "test-switches": "test_switches_rom.py", "test-sector-movers": "test_sector_movers_rom.py",
    "test-level-script": "test_level_script_rom.py", "test-level-runtime": "test_level_runtime.py",
    "test-entity-pool": "test_entity_pool_rom.py", "test-enemy-activation": "test_enemy_activation_rom.py",
    "test-script-spawn": "test_script_spawn.py",
    "test-enemy-motion": "test_enemy_motion_rom.py", "test-enemy-navigation": "test_enemy_navigation_rom.py",
    "test-enemy-lifecycle": "test_enemy_lifecycle_rom.py", "test-enemy-cd": "test_enemy_cd_rom.py",
    "test-enemy-cd-runtime": "test_enemy_cd_runtime.py",
    "test-enemy-targeting": "test_enemy_targeting_rom.py",
    "test-ray-traversal": "test_ray_traversal_rom.py",
    "test-ray-intercepts": "test_ray_intercepts_rom.py",
    "test-enemy-ray": "test_enemy_ray_rom.py",
    "test-enemy-queries": "test_enemy_queries.py",
    "test-combat-damage": "test_combat_damage_rom.py",
    "test-combat-explosion": "test_combat_explosion_rom.py",
    "test-combat-rewards": "test_combat_rewards_rom.py",
    "test-combat-combined": "test_combat_combined_rom.py",
    "test-combat-runtime": "test_combat_runtime.py",
    "test-weapon-animation": "test_weapon_animation_rom.py",
    "test-weapon-controller": "test_weapon_controller_rom.py",
    "test-weapon-aim": "test_weapon_aim_rom.py",
    "test-weapon-hit": "test_weapon_hit_rom.py",
    "test-weapon-runtime": "test_weapon_runtime.py",
    "test-projectile-create": "test_projectile_create_rom.py",
    "test-projectile-motion": "test_projectile_motion_rom.py",
    "test-projectile-controller": "test_projectile_controller_rom.py",
    "test-projectile-bounce": "test_projectile_bounce_rom.py",
    "test-projectile-runtime": "test_projectile_runtime.py",
    "test-gunrilla": "test_gunrilla.py",
    "test-projectile-effect": "test_projectile_effect_rom.py",
    "test-projectile-conversion": "test_projectile_conversion.py",
    "test-entity-dispatch": "test_entity_dispatch_rom.py",
    "test-entity-runtime": "test_entity_runtime.py",
    "test-weapon-loadout": "test_weapon_loadout_rom.py",
    "test-weapon-progress": "test_weapon_progress_rom.py",
    "test-statistics": "test_statistics_rom.py", "test-native-statistics": "test_native_statistics.py",
    "test-completion": "test_completion_rom.py", "test-native-completion": "test_native_completion.py",
    "test-pause": "test_pause_rom.py", "test-intermission": "test_intermission.py",
    "test-campaign-story": "test_campaign_story_rom.py",
    "test-campaign-screen": "test_campaign_screen.py",
    "test-netricsa": "test_netricsa.py",
    "test-umt-audio": "test_umt_audio.py",
    "test-hud-notice": "test_hud_notice_rom.py", "test-native-notices": "test_native_notices.py",
    "test-prion-targeting": "test_prion_targeting.py",
    "test-weapon-start": "test_weapon_start.py",
    "test-frame": "test_frame_rom.py",
    "test-sprite-dispatch": "test_sprite_dispatch_rom.py",
    "test-actor-animation": "test_actor_animation_rom.py",
    "test-actor-animation-runtime": "test_actor_animation_runtime.py",
    "test-pickup-respawn": "test_pickup_respawn_rom.py",
    "test-pickup-respawn-runtime": "test_pickup_respawn_runtime.py",
    "test-game-runtime": "test_game_runtime.py",
    "test-map-gameplay-ownership": "test_map_gameplay_ownership.py",
    "test-native-game": "test_native_game.py",
    "test-native-playability": "test_native_playability.py",
    "test-second-level": "test_second_level.py",
    "test-third-level": "test_third_level.py",
    "test-campaign": "test_campaign.py",
    "test-map-view": "test_map_view.py",
    "test-map-view-rom": "test_map_view_rom.py",
    "test-menu": "test_menu.py",
    "test-profile": "test_profile.py",
    "test-menu-rom": "test_menu_rom.py",
    "test-pc-renderer": "test_pc_renderer.py",
    "test-free-look": "test_free_look.py",
    "test-chase-aim": "test_chase_aim.py",
    "test-gba-aim": "test_gba_aim.py",
    "test-unlimited-range": "test_unlimited_range.py",
    "test-audio": "test_audio.py",
    "test-audio-playability": "test_audio_playability.py",
    "test-pc-renderer-playability": "test_pc_playability.py",
    "test-enemy-control": "test_enemy_control_rom.py",
    "test-enemy-control-runtime": "test_enemy_control_runtime.py",
    "test-enemy-ranged": "test_enemy_ranged_rom.py",
    "test-enemy-boss": "test_enemy_boss_rom.py",
    "test-enemy-boss-runtime": "test_enemy_boss_runtime.py",
    "test-weapon-fixes": "test_weapon_fixes.py",
    "test-special-weapons": "test_special_weapons.py",
    "test-palette-effects": "test_palette_effect_rom.py",
    "test-pickup-runtime": "test_pickup_runtime.py",
    "test-particle-create": "test_particle_create_rom.py",
    "test-particle-runtime": "test_particle_runtime.py",
    "test-projectile-target": "test_projectile_target_rom.py",
    "test-projectile-extended": "test_projectile_extended_rom.py",
    "test-input-dispatch": "test_input_dispatch_rom.py",
    "test-pc-input": "test_pc_input.py",
    "test-sprite-assets": "test_sprite_assets.py",
    "test-mode-control": "test_mode_control_rom.py",
    "test-raster-fill": "test_raster_fill_rom.py",
    "test-texture-assets": "test_texture_assets.py",
    "test-map-texture-bindings": "test_map_texture_bindings_rom.py",
    "test-world-raster": "test_world_raster_rom.py",
    "test-render-view": "test_render_view_rom.py",
    "test-plane-renderer": "test_plane_renderer_rom.py",
    "test-world-visibility": "test_world_visibility_rom.py",
    "test-wall-renderer": "test_wall_renderer_rom.py",
    "test-scene-coverage": "test_scene_coverage_rom.py",
    "test-world-scene": "test_world_scene.py",
    "test-world-scene-rom": "test_world_scene_rom.py",
    "test-map-actors": "test_map_actors_rom.py",
    "test-actor-assets": "test_actor_assets.py",
    "test-actor-projection": "test_actor_projection_rom.py",
    "test-actor-queue": "test_actor_queue_rom.py",
    "test-actor-scene-rom": "test_actor_scene_rom.py",
    "test-actor-scene": "test_actor_scene.py",
    "test-sprite-cache": "test_sprite_cache_rom.py",
    "test-sprite-pool": "test_sprite_pool_rom.py",
    "test-sprite-oam": "test_sprite_oam_rom.py",
    "test-display": "test_display.py",
    "test-hud-colors": "test_hud_colors.py",
    "test-hud-scene": "test_hud_scene.py",
    "test-hud-scene-rom": "test_hud_scene_rom.py",
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=["setup", "import", "status", "autopilot", "export", "rebuild", "tool", "gui", "match", *TESTS])
    parser.add_argument("--own-copy", action="store_true")
    parser.add_argument("--name")
    parser.add_argument("--args", default="{}")
    parser.add_argument("--args-file", type=Path)
    parser.add_argument("--source", type=Path)
    parser.add_argument("--analysis-cache", type=Path,
                        help="reuse a conservative Luvdis pass from the same ROM")
    args = parser.parse_args()
    if args.command in TESTS:
        import runpy
        filename = TESTS[args.command]
        runpy.run_path(str(ROOT / "tests" / filename), run_name="__main__")
        return
    if args.command == "setup":
        from decompnet import toolchains
        for tool in ("luvdis", "agbcc", "m2c"):
            toolchains.install(tool, log=lambda line: print(line, flush=True))
        import runpy
        runpy.run_path(str(ROOT / "scripts" / "patch_luvdis.py"))
        return
    if args.command == "gui":
        from decompnet.app.cli import main as cli
        import sys
        sys.argv = ["decomp", "--no-browser"]
        cli()
        return
    from decompnet.app.engine import RomNetwork
    home = ROOT / "decomp"
    (home / ".decomp").mkdir(parents=True, exist_ok=True)
    net = RomNetwork(home / ".decomp" / "decomp.db", home)
    state = home / "project.json"
    if args.command == "import":
        if not args.own_copy:
            parser.error("import requires --own-copy after confirming ownership")
        if args.analysis_cache:
            import hashlib
            import re
            from decompnet.rom import gba
            cache = args.analysis_cache.resolve()
            original = (ROOT / "orig/SSA.gba").read_bytes()
            if hashlib.sha256((cache / "rom.gba").read_bytes()).digest() != hashlib.sha256(original).digest():
                parser.error("analysis cache belongs to a different ROM")
            assembly = (cache / "rom.s").read_text()
            config = (cache / "functions.cfg").read_text()
            found = sorted({int(m.group(1), 16) for m in re.finditer(
                r"^(?:[a-z_]+[ \t]+)?(0x[0-9A-Fa-f]+)", config, re.M)})
            if not found or len(assembly) < len(original):
                parser.error("analysis cache appears incomplete")
            def conservative_pass(rom_path, workdir, rounds=0):
                (workdir / "rom.s").write_text(assembly)
                (workdir / "functions.cfg").write_text(config)
                return assembly, found
            gba.find_functions = conservative_pass
            print(f"Using conservative catalogue: {len(found)} discovered functions; coverage is incomplete", flush=True)
        else:
            from decompnet.rom import gba
            discover = gba.find_functions
            def conservative_pass(rom_path, workdir, rounds=0):
                return discover(rom_path, workdir, rounds=0)
            gba.find_functions = conservative_pass
            print("Using conservative Luvdis discovery; coverage is incomplete", flush=True)
        slug = net.import_rom(ROOT / "orig" / "SSA.gba", confirmed=True,
                              log=lambda line: print(line, flush=True))
        state.write_text(json.dumps({"slug": slug, "discovery": "conservative cached Luvdis pass" if args.analysis_cache
                                    else "conservative Luvdis pass", "complete_code_coverage": False}, indent=2) + "\n")
    if not state.exists():
        parser.error("import the ROM first")
    slug = json.loads(state.read_text())["slug"]
    if args.command == "autopilot":
        net.start_autopilot(slug, workers=3)
        while True:
            status = net.autopilot_status(slug)
            print(json.dumps(status), flush=True)
            if status["state"] != "running":
                break
            time.sleep(10)
    elif args.command == "export":
        import zipfile
        source = Path(net._project(slug)["root"]) / "source"
        if not source.resolve().is_relative_to(home.resolve()):
            raise SystemExit("Refusing to regenerate a source tree outside the workspace decomp directory")
        if source.exists():
            backups = home / "backups"
            backups.mkdir(exist_ok=True)
            backup = backups / f"{slug}-source-{time.time_ns()}.zip"
            with zipfile.ZipFile(backup, "w", compression=zipfile.ZIP_DEFLATED) as archive:
                for path in source.rglob("*"):
                    rel = path.relative_to(source)
                    if path.is_file() and rel.parts[0] not in ("build", "asm", ".git") and not rel.name.startswith("baserom"):
                        archive.write(path, rel.as_posix())
            print(f"Backed up source files to {backup}", flush=True)
        result = net.export_rom(slug, log=lambda line: print(line, flush=True))
        print(json.dumps(result, indent=2), flush=True)
        (home / "export-result.json").write_text(json.dumps(result, indent=2) + "\n")
        if not result.get("identical"):
            raise SystemExit(1)
    elif args.command == "rebuild":
        import hashlib
        import subprocess
        import sys
        project = net._project(slug)
        source = Path(project["root"]) / "source"
        process = subprocess.run([sys.executable, "-u", "build.py"], cwd=source,
                                 capture_output=True, text=True)
        print(process.stdout + process.stderr, end="", flush=True)
        output = source / "build/game.gba"
        identical = process.returncode == 0 and output.exists() and output.read_bytes() == (ROOT / "orig/SSA.gba").read_bytes()
        config = json.loads((source / "build.json").read_text())
        result = {"path": str(source), "identical": identical,
                  "matched": sum(bool(f.get("c")) for f in config["functions"]),
                  "functions": len(config["functions"]), "log": process.stdout + process.stderr,
                  "verified_at": time.time()}
        if output.exists():
            result["sha1"] = hashlib.sha1(output.read_bytes()).hexdigest()
        (home / "export-result.json").write_text(json.dumps(result, indent=2) + "\n")
        (ROOT / "analysis/rebuild-validation.json").write_text(json.dumps(result, indent=2) + "\n")
        with net.store.tx() as db:
            cfg = json.loads(db.execute("SELECT config FROM projects WHERE id=?", (project["id"],)).fetchone()["config"])
            cfg["export"] = result
            db.execute("UPDATE projects SET config=? WHERE id=?", (json.dumps(cfg), project["id"]))
        if not identical:
            raise SystemExit(1)
    elif args.command in ("tool", "match"):
        from decompnet.app.tools import call
        agent, _ = net.local_agent("codex-session")
        if args.command == "match":
            if not args.source or not args.name:
                parser.error("match requires --name FUNCTION --source FILE")
            result = call(net, agent, slug, "compile_and_diff", {"function": args.name,
                          "source": args.source.read_text(encoding="utf-8-sig")})
        else:
            payload = args.args_file.read_text(encoding="utf-8-sig") if args.args_file else args.args
            result = call(net, agent, slug, args.name, json.loads(payload))
        print(result, flush=True)
    else:
        print(json.dumps(net.overview(slug), indent=2), flush=True)


if __name__ == "__main__":
    main()
