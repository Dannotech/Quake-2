"""Compare Release x86/x64 OpenGL timedemos under CDB, with bounded runs.

Run after building both platforms. Assets must exist in x64/Release/baseq2.
Logs, isolated game directories, and results are saved under the output folder.
"""
import argparse
import csv
import hashlib
import json
import os
from pathlib import Path
import random
import re
import shutil
import statistics
import subprocess
import time

REPO = Path(__file__).resolve().parents[2]
MODES = [(3, 640, 480), (4, 800, 600), (6, 1024, 768), (10, 1280, 720),
         (11, 1600, 900), (12, 1920, 1080), (13, 2560, 1440), (15, 3840, 2160)]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=REPO / 'x64/benchmarks/opengl')
    parser.add_argument('--repeats', type=int, default=7)
    parser.add_argument('--demo', default='demo1.dm2')
    parser.add_argument('--modes', nargs='+', type=int, choices=[mode[0] for mode in MODES])
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    modes_to_test = [mode for mode in MODES if not args.modes or mode[0] in args.modes]
    debugger_root = Path(os.environ['ProgramFiles(x86)']) / 'Windows Kits/10/Debuggers'
    if args.repeats < 3:
        parser.error('Use at least three measured runs.')
    paks = list((REPO / 'x64/Release/baseq2').glob('*.pak'))
    if not paks:
        parser.error('Original game PAKs are required in x64/Release/baseq2.')
    guard = (REPO / 'build/vs2026/no_dialogs.cdb').read_text()
    hashes = {}
    for arch, source in [('x86', REPO / 'Release'), ('x64', REPO / 'x64/Release')]:
        destination = output / arch
        base = destination / 'baseq2'
        base.mkdir(parents=True, exist_ok=True)
        for filename in ['quake2.exe', 'quake2.pdb', 'ref_gl.dll', 'ref_gl.pdb', f'game{arch}.dll']:
            file = source / filename
            if file.exists():
                shutil.copy2(file, destination / filename)
                hashes[f'{arch}/{filename}'] = hashlib.sha256(file.read_bytes()).hexdigest()
        for pak in paks:
            if not (base / pak.name).exists():
                os.link(pak, base / pak.name)
        commands = guard
        if arch == 'x86':
            commands = commands.replace('da @rcx', 'da poi(@esp+4)')
            commands = commands.replace('da @rdx', 'da poi(@esp+8)').replace('du @rdx', 'du poi(@esp+8)')
        (destination / 'guard.cdb').write_text(commands)
    metadata = {
        'demo': args.demo, 'repeats': args.repeats, 'driver': 'opengl32',
        'vsync': False, 'audio': False, 'hashes': hashes,
        'modes': modes_to_test, '4k_fullscreen': True,
        'timing': 'engine timedemo; -benchmark bypasses 1ms wait, fixed 16ms simulation step',
    }
    (output / 'metadata.json').write_text(json.dumps(metadata, indent=2))
    rows = []
    driver_identity = None

    def run(arch, mode, phase, iteration):
        nonlocal driver_identity
        mode_id, width, height = mode
        directory = output / arch
        # Fresh identical defaults for each run; no user config is modified.
        config = directory / 'baseq2/config.cfg'
        if config.exists():
            config.unlink()
        settings = {
            'vid_ref': 'gl', 'gl_driver': 'opengl32', 'vid_fullscreen': int(width == 3840),
            'vid_xpos': 0, 'vid_ypos': 0, 'gl_mode': mode_id, 'gl_swapinterval': 0,
            'gl_ext_swapinterval': 1, 'gl_finish': 0, 'gl_picmip': 0,
            'gl_texturemode': 'GL_LINEAR_MIPMAP_NEAREST', 'gl_ztrick': 0,
            'viewsize': 100, 's_initsound': 0, 'cd_nocd': 1, 'cl_introPlayed': 1,
            'logfile': 2, 'timedemo': 1,
        }
        (directory / 'baseq2/bench.cfg').write_text(
            '\n'.join(f'set {key} {value}' for key, value in settings.items()) +
            f'\ndemomap {args.demo}\nset nextserver "disconnect; quit"\n')
        # Apply renderer settings before initialization, then run the config.
        command = [str(debugger_root / arch / 'cdb.exe'), '-y', '.', '-G', '-cf',
                   'guard.cdb', '.\\quake2.exe']
        if phase != 'stock':
            command.append('-benchmark')
        for key in ['vid_ref', 'gl_driver', 'vid_fullscreen', 'gl_mode', 's_initsound',
                    'cd_nocd', 'gl_swapinterval', 'logfile']:
            command.extend(['+set', key, str(settings[key])])
        command.extend(['+exec', 'bench.cfg'])
        label = f'{phase}-{arch}-{width}x{height}-{iteration}'
        started = time.monotonic()
        with (output / f'{label}.debug.log').open('w') as log:
            process = subprocess.Popen(command, cwd=directory, stdout=log, stderr=subprocess.STDOUT,
                                       creationflags=subprocess.CREATE_NO_WINDOW)
            try:
                process.wait(timeout=30)
            except subprocess.TimeoutExpired:
                subprocess.run(['taskkill', '/PID', str(process.pid), '/T', '/F'],
                               capture_output=True, check=False)
                raise RuntimeError(f'{label}: timed out')
        game_log = (directory / 'baseq2/qconsole.log').read_text(errors='replace')
        debug_log = (output / f'{label}.debug.log').read_text(errors='replace')
        (output / f'{label}.game.log').write_text(game_log)
        failures = re.findall(r'^FAILURE:.*', debug_log, re.M)
        matches = re.findall(r'^(\d+) frames, ([\d.]+) seconds: ([\d.]+) fps', game_log, re.M)
        actual_modes = re.findall(r'setting mode \d+: (\d+) (\d+) ([WF])', game_log)
        renderer = re.search(r'^GL_RENDERER: (.+)$', game_log, re.M)
        version = re.search(r'^GL_VERSION: (.+)$', game_log, re.M)
        identity = (renderer[1].strip(), version[1].strip()) if renderer and version else None
        if driver_identity is None and identity:
            driver_identity = identity
            metadata['renderer'], metadata['gl_version'] = identity
            (output / 'metadata.json').write_text(json.dumps(metadata, indent=2))
        if (failures or len(matches) != 1 or not actual_modes or
                actual_modes[-1] != (str(width), str(height), 'F' if width == 3840 else 'W') or
                identity is None or identity != driver_identity):
            raise RuntimeError(f'{label}: invalid run; inspect saved logs ({failures})')
        frames, seconds, fps = matches[0]
        row = dict(phase=phase, arch=arch, width=width, height=height, iteration=iteration,
                   frames=int(frames), seconds_reported=float(seconds), fps=float(fps),
                   process_seconds=round(time.monotonic() - started, 3))
        rows.append(row)
        with (output / 'runs.csv').open('w', newline='') as file:
            writer = csv.DictWriter(file, fieldnames=row.keys())
            writer.writeheader()
            writer.writerows(rows)
        print(f'{label}: {fps} FPS ({frames} frames)', flush=True)

    for mode in modes_to_test:
        for arch in ['x86', 'x64']:
            run(arch, mode, 'stock', 0)
            run(arch, mode, 'warmup', 0)
    randomizer = random.Random(20261010)
    for iteration in range(1, args.repeats + 1):
        modes = list(modes_to_test)
        randomizer.shuffle(modes)
        for mode in modes:
            for arch in (['x86', 'x64'] if iteration % 2 else ['x64', 'x86']):
                run(arch, mode, 'measured', iteration)
    summaries = []
    for _, width, height in modes_to_test:
        values = {arch: [row['fps'] for row in rows if row['phase'] == 'measured' and
                        row['arch'] == arch and row['width'] == width and row['height'] == height]
                  for arch in ['x86', 'x64']}
        medians = {arch: statistics.median(fps) for arch, fps in values.items()}
        summaries.append(dict(resolution=f'{width}x{height}', x86_fps=medians['x86'],
                              x64_fps=medians['x64'],
                              change_percent=round((medians['x64'] / medians['x86'] - 1) * 100, 2),
                              ranges={arch: [min(fps), max(fps)] for arch, fps in values.items()}))
    (output / 'summary.json').write_text(json.dumps(summaries, indent=2))
    print(json.dumps(summaries, indent=2), flush=True)


if __name__ == '__main__':
    main()
