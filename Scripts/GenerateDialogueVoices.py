"""Generate expressive synthetic Chapter One performances with local Qwen3 VoiceDesign.

Generation stages takes outside Content. --install copies a complete verified set into
Content; then run ImportDialogueVoices.py in Unreal. --check requires no model or network.
"""
from __future__ import annotations

import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import wave

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "Source/AshesOfHeaven/Gameplay/Chapter/AHLevelOneNarrative.cpp"
OUTPUT = ROOT / "Content/Ashes/Audio/Dialogue"
STAGING = ROOT / "Saved/VoiceGeneration/Expressive"
MANIFEST = ROOT / "Docs/DialogueVoices.json"
APPROVED_CHILD = ROOT / "SourceAudio/Dialogue/Child_Ana_Subtle.mp3"
MODEL = "mlx-community/Qwen3-TTS-12Hz-1.7B-VoiceDesign-8bit"
# Stable casting descriptions and per-character seeds keep successive takes close in timbre.
# VoiceDesign can still vary between takes; audition after changing model or direction.
CAST = {
    "CHILD": (1307, "A little boy, seven years old, with a genuinely small prepubescent boy voice, thin breathy timbre and youthful imperfect articulation. Natural American English."),
    "LUCIAN": (2051, "A battle-worn man in his late thirties. Low masculine chest voice, dark slightly rough baritone, neutral American English. Restrained, exhausted military commander."),
    "MAYA": (3113, "A woman in her late twenties, grounded clear mid-low female voice, slightly husky, neutral American English. Brave frontline combat engineer. Alert, emotionally present, direct and human."),
    "SAEL": (4019, "A male admiral in his mid-fifties, deep resonant authoritative bass, precise educated British English. Calm authority with heavy controlled gravity and painful moral conviction."),
    "IVO": (5009, "A man in his early thirties, warm rough mid-range masculine voice, light northern English accent. Resourceful armored vehicle pilot. Casual military warmth and dry humor under exhaustion."),
    "OTHER LUCIAN": (2051, "A battle-worn man in his late thirties. Low masculine chest voice, dark slightly rough baritone, neutral American English. An impossible duplicate of a military commander. Intimate, haunted, soft accusatory certainty with years of buried grief."),
    "NYSA": (7013, "A young adult woman with a soft breathy intimate mid-high voice, natural American English. An unknown voice reaching a loved one across an impossible distance."),
    "CIVILIAN": (8011, "An elderly woman with a fragile thin slightly raspy voice, natural British English. Shell-shocked survivor recognizing a terrifying presence."),
    "STATION ANNOUNCEMENT": (9001, "An adult female public transport announcer with clear British English diction and a calm measured professional voice. Human recorded station announcement with clean articulation."),
}
SCENES = {
    "Opening": "After a terrible battle. Intimate, quiet, wounded hope. A child seeks reassurance; the soldier hides bitter sadness.",
    "VeilRevelation": "A survivor reveals a horrifying truth. Confusion turns into frightened realization. Speak to nearby people, not to an audience.",
    "FailsafeOrder": "An urgent military transmission reveals the threat to all humanity. Grave, tense exchange. Lucian needs answers, Maya is horrified, Sael is burdened but resolute.",
    "OtherLucianFirst": "An impossible encounter. The duplicate speaks with cold haunted certainty; Lucian is wary and confused; Maya anxiously calls to him.",
    "OtherLucianSecond": "Immediate danger. Maya shouts for Lucian to move, fear driving urgent force.",
    "ErebusOpening": "Two exhausted soldiers reconnect after one was knocked unconscious. Dry affectionate sarcasm gives way to alert concern as urgent fleet orders arrive.",
    "TransitStation": "An abandoned evacuation station. Ivo is relieved his friend survived and uses dry humor to hide nerves. Maya's question is skeptical banter.",
    "OpenBattlefield": "Combat radio coordination. Ivo offers confident practical reassurance with dry humor. Maya answers with skeptical warmth.",
    "ManticoreSection": "Friends regroup at a battered vehicle. Warm teasing military banter; Ivo admires the ancient Cathedral with awe, Lucian brings attention back to the mission.",
    "CathedralApproach": "The vehicle stops obeying its pilot. Confusion shifts to tense fear and hushed realization that something else is in control.",
    "FailsafeTerminal": "Eleven million civilian lives are at stake. Maya pleads, horrified and near tears. Lucian is weighed down by the exact number. Sael answers with grave restrained conviction, no triumph.",
    "Escape": "The world is coming apart. Immediate alarm and desperate urgency. Short lines are forceful and frightened.",
    "ErebusDestruction": "Witnessing the death of a world. Maya is devastated, close to tears; Lucian is hollow and grief-stricken. Sael's confirmation is sober, not victorious. Nysa calls intimately with aching recognition.",
}
# These directions preserve the audition wording and emphasize pivotal emotional beats.
OVERRIDES = {
    ("MAYA", "Ma'am? Civil Defense. Can you hear me?"): "A woman in her late twenties, grounded clear mid-low female voice, slightly husky, neutral American English. Brave frontline combat engineer gently trying to reach an unknown elderly female survivor in shock. Concerned, alert, compassionate, speaking clearly. Pronounce Ma'am with the short A vowel in ham; she is addressing a stranger respectfully. Natural dramatic acting.",
    ("CHILD", "Did we win?"): "A little boy, seven years old, with a genuinely small prepubescent boy voice, thin breathy timbre and youthful imperfect articulation. Natural American English. He has survived a terrifying battle and quietly asks a trusted soldier whether it is finally over. Tentative, frightened, desperately hopeful. Soft rising question with a little tremble. Natural intimate dramatic acting.",
    ("LUCIAN", "For a while."): "A battle-worn man in his late thirties. Low masculine chest voice, dark slightly rough baritone, neutral American English. Restrained, exhausted military commander speaking intimately to a frightened child. Bitter sadness hidden behind gentle reassurance. Say the short answer softly, with a weary breath and a faint tragic hesitation. Natural cinematic acting.",
    ("MAYA", "Don't do this."): "A woman in her late twenties, grounded clear mid-low female voice, slightly husky, neutral American English. Brave frontline combat engineer. A quiet desperate plea to her trusted commander, who is about to kill millions. Voice almost breaking on the final words. Natural emotionally vivid dramatic acting, not narration.",
    ("NYSA", "Lucian."): "A young adult woman with a soft breathy intimate mid-high voice, natural American English. An unknown voice reaching a loved one across an impossible distance. Whisper the name Loo-shun with aching recognition, warmth and sadness. Gentle emotionally expressive human voice, not singing.",
    ("CIVILIAN", "It remembers us."): "An elderly woman with a fragile thin slightly raspy voice, natural British English. Shell-shocked survivor recognizing a terrifying presence. A hushed trembling realization, genuine fear held inside the words, slightly broken breath. Natural dramatic acting, clear words.",
}
LINE_PATTERN = re.compile(r'NarrativeLine\(TEXT\(("(?:\\.|[^"\\])*")\),\s*TEXT\(("(?:\\.|[^"\\])*")\),\s*([\d.]+)f\)')
SCENE_PATTERN = re.compile(r'TArray<FAHDialogueLine> (\w+)\(\)|case EAHChapterStage::(\w+):')


def asset_name(speaker: str, text: str) -> str:
    digest = hashlib.md5((speaker + "\n" + text).encode("utf-8")).hexdigest()
    return "SW_" + speaker.replace(" ", "_") + "_" + digest


def collect_lines() -> list[dict]:
    source = SOURCE.read_text()
    matches = list(LINE_PATTERN.finditer(source))
    if len(matches) != source.count("NarrativeLine(TEXT("):
        raise ValueError("A narrative line was not parsed; update the extractor.")
    lines = {}
    for match in matches:
        speaker, text = json.loads(match[1]), json.loads(match[2])
        seed, description = CAST[speaker]
        previous = list(SCENE_PATTERN.finditer(source[:match.start()]))[-1]
        scene = previous[1] or previous[2]
        direction = OVERRIDES.get((speaker, text), description + " " + SCENES[scene] +
            " Natural emotionally expressive dramatic acting. Speak only the supplied words, no added dialogue or singing.")
        if speaker == "STATION ANNOUNCEMENT":
            direction = description + " Even polite reassurance, unhurried cadence. Abruptly interrupted at the dash."
        name = asset_name(speaker, text)
        lines[name] = dict(speaker=speaker, text=text, asset_name=name,
            asset_path=f"/Game/Ashes/Audio/Dialogue/{name}.{name}", model=MODEL,
            voice=f"{speaker.lower().replace(' ', '_')}_{seed}", seed=seed,
            direction=direction, scene=scene, source_line=source[:match.start()].count("\n") + 1)
        if speaker == "CHILD" and text == "Did we win?":
            lines[name].update(model="Microsoft Edge TTS", voice="en-US-AnaNeural",
                seed=0, direction="User-approved Ana Subtle: rate -10%, pitch -8Hz.",
                approved_source=str(APPROVED_CHILD.relative_to(ROOT)),
                source_sha256=hashlib.sha256(APPROVED_CHILD.read_bytes()).hexdigest())
    if not lines:
        raise ValueError("No dialogue found")
    return list(lines.values())


def inspect_wave(path: Path) -> float:
    with wave.open(str(path), "rb") as wav:
        if (wav.getnchannels(), wav.getsampwidth(), wav.getframerate()) != (1, 2, 48000):
            raise ValueError(f"Expected mono 16-bit 48 kHz: {path}")
        frames = wav.readframes(wav.getnframes())
        if len(frames) < 4800 or not any(frames):
            raise ValueError(f"Empty or silent take: {path}")
        return len(frames) / 96000.0


def generate(line: dict, model) -> dict:
    if line.get("approved_source"):
        destination = STAGING / (line["asset_name"] + ".wav")
        subprocess.run(["ffmpeg", "-v", "error", "-y", "-i", str(ROOT / line["approved_source"]),
            "-ar", "48000", "-ac", "1", "-c:a", "pcm_s16le", str(destination)], check=True)
        return dict(line, duration=inspect_wave(destination))
    import mlx.core as mx
    import numpy as np
    destination = STAGING / (line["asset_name"] + ".wav")
    receipt = destination.with_suffix(".json")
    signature = {k: line[k] for k in ("text", "speaker", "model", "seed", "direction")}
    if destination.exists() and receipt.exists() and json.loads(receipt.read_text()) == signature:
        return dict(line, duration=inspect_wave(destination))
    mx.random.seed(line["seed"])
    results = list(model.generate_voice_design(text=line["text"], instruct=line["direction"],
        language="English", max_tokens=500, temperature=0.7, verbose=False))
    audio = np.concatenate([np.asarray(r.audio).reshape(-1) for r in results])
    rate = results[0].sample_rate
    if len(audio) / rate >= 39.5:
        raise ValueError(f"Take reached generation limit: {line['asset_name']}")
    with tempfile.TemporaryDirectory() as temporary:
        raw = Path(temporary) / "take.wav"
        with wave.open(str(raw), "wb") as wav:
            wav.setnchannels(1); wav.setsampwidth(2); wav.setframerate(rate)
            wav.writeframes((np.clip(audio, -1, 1) * 32767).astype("<i2").tobytes())
        subprocess.run(["ffmpeg", "-v", "error", "-y", "-i", str(raw), "-af",
            "loudnorm=I=-19:TP=-2:LRA=11", "-ar", "48000", "-ac", "1", "-c:a", "pcm_s16le", str(destination)], check=True)
    receipt.write_text(json.dumps(signature, indent=2) + "\n")
    duration = inspect_wave(destination)
    print(f"{line['speaker']} | {line['text']} | {duration:.2f}s", flush=True)
    mx.clear_cache()
    return dict(line, duration=duration)


def validate(manifest: dict, folder: Path, lines: list[dict]) -> None:
    if {x['asset_name'] for x in manifest['lines']} != {x['asset_name'] for x in lines}:
        raise ValueError("Voice manifest is stale")
    desired = {x['asset_name']: x for x in lines}
    for line in manifest['lines']:
        if any(line.get(key) != desired[line['asset_name']][key]
               for key in ('text', 'speaker', 'model', 'seed', 'direction')):
            raise ValueError(f"Voice direction changed; regenerate {line['asset_name']}")
        if line.get("source_sha256") != desired[line['asset_name']].get("source_sha256"):
            raise ValueError("Approved recording changed; restage dialogue")
        duration = inspect_wave(folder / (line['asset_name'] + '.wav'))
        if abs(duration - line['duration']) > 0.01:
            raise ValueError(f"Manifest duration mismatch: {line['asset_name']}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--install", action="store_true")
    args = parser.parse_args()
    lines = collect_lines()
    print(f"{len(lines)} unique lines; {dict(Counter(x['speaker'] for x in lines))}", flush=True)
    if args.check:
        validate(json.loads(MANIFEST.read_text()), OUTPUT, lines)
        print("All authored dialogue has a valid source recording.")
        return
    if args.install:
        manifest = json.loads((STAGING / "manifest.json").read_text())
        validate(manifest, STAGING, lines)
        if manifest['model'] != MODEL:
            raise ValueError("Staged takes use a different model")
        backup = ROOT / "Saved/VoiceGeneration/BeforeExpressive"
        backup.mkdir(exist_ok=True)
        for line in lines:
            name = line['asset_name'] + '.wav'
            if (OUTPUT / name).exists() and not (backup / name).exists():
                shutil.copy2(OUTPUT / name, backup / name)
            shutil.copy2(STAGING / name, OUTPUT / name)
        if MANIFEST.exists() and not (backup / 'manifest.json').exists():
            shutil.copy2(MANIFEST, backup / 'manifest.json')
        MANIFEST.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n")
        print("Installed source takes. Reimport in Unreal to update SoundWaves.")
        return
    from mlx_audio.tts.utils import load_model
    STAGING.mkdir(parents=True, exist_ok=True)
    model = load_model(MODEL)
    generated = [generate(line, model) for line in lines]
    manifest = dict(disclosure="AI-generated character voices", provider="Local Qwen3 VoiceDesign via MLX",
        model=MODEL, cast={k: dict(voice=f"{k.lower().replace(' ', '_')}_{v[0]}", direction=v[1]) for k, v in CAST.items()}, lines=generated)
    child = next(x for x in generated if x['speaker'] == 'CHILD')
    manifest['cast']['CHILD'] = dict(voice=child['voice'], direction=child['direction'])
    manifest['provider'] = "Local Qwen3 VoiceDesign via MLX; approved child: Microsoft Edge TTS"
    validate(manifest, STAGING, lines)
    (STAGING / "manifest.json").write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n")
    print(f"Staged {len(generated)} performances, {sum(x['duration'] for x in generated):.1f}s. Audition before --install.")


if __name__ == "__main__":
    main()
