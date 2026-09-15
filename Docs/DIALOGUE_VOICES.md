# Chapter One voices

Chapter One has 80 unique spoken lines across nine synthetic English character voices.
The adult cast uses Qwen3 VoiceDesign with character descriptions and scene directions.
The child uses the user-approved **Ana Subtle** take: Microsoft `en-US-AnaNeural`,
rate -10%, pitch -8 Hz. The exact audition is preserved in
`SourceAudio/Dialogue/Child_Ana_Subtle.mp3` and decoded to mono 48 kHz PCM for Unreal.
This is an approved vocal sound, not a verified speaker age. Subtitles remain available.

Earlier child takes and auditions were rejected. The remaining cast was judged only a
modest improvement; further acting refinement remains separate from this child approval.
Audio coverage tests verify wiring, not acting quality.

| Role | Casting direction |
| --- | --- |
| Child | Approved Ana Subtle girl voice, slightly lowered pitch |
| Lucian | Worn male baritone, restrained grief and dry warmth |
| Maya | Grounded female voice, banter developing into horrified pleading |
| Sael | Mature British male bass, controlled authority and moral burden |
| Ivo | Warm northern English male, relieved humor and growing alarm |
| Other Lucian | Lucian-like male baritone, intimate accusation and haunted certainty |
| Nysa | Soft young adult female, aching recognition |
| Civilian | Elderly woman, hushed shock and trembling realization |
| Station announcement | Calm recorded female British public-address voice |

These are AI-generated performances, not human actor recordings. VoiceDesign can vary
between takes even with matching seed and character description; audition casting and
performance when changing direction. The earlier Kokoro takes were rejected for mechanical
prosody and unsuitable casting. They are backed up under Saved/VoiceGeneration/BeforeExpressive.

## Runtime

The game plays normal SoundWaves, offline; it never loads speech models or API credentials.
WAVs and imported assets live in Content/Ashes/Audio/Dialogue. They are mono 48 kHz / 16-bit
PCM. Adult takes are normalized toward -19 LUFS with a -2 dB true-peak ceiling.
The approved child preserves the audition level and uses the synthesis pitch setting above;
no radio filter or echo is baked in.
Assets play once, load inline, and route through SM_Dialogue. Existing ambience ducking applies.

Docs/DialogueVoices.json records exact text, model, direction, seed, scene, asset path and
duration. JSON stays outside Content because Unreal otherwise opens DataTable import dialogs.
The canonical narrative resolves clips using MD5 of UTF-8 speaker + newline + text. Text edits
require new takes. Subtitle timing uses the longer of authored duration and recording plus
0.15 seconds; the finale hold uses those same durations. /Game/Ashes is already always cooked.

## Regenerate

Use an Apple Silicon Python environment with mlx-audio==0.5.3 and ffmpeg. This workstation's
environment is Saved/VoiceGeneration/.venv. Models download to the Hugging Face cache and
are not distributed with the game.

1. Run Saved/VoiceGeneration/.venv/bin/python Scripts/GenerateDialogueVoices.py.
   This stages all takes and metadata under Saved/VoiceGeneration/Expressive. Matching
   existing takes are reused. Edit CAST, SCENES or OVERRIDES to adjust performance.
2. Audition staged takes. Run python3 Scripts/GenerateDialogueVoices.py --install to copy
   the complete set into Content and update Docs/DialogueVoices.json.
3. With PIE stopped, execute Scripts/ImportDialogueVoices.py in the editor's Python console
   or remote Python client. It verifies durations, routes audio, adds metadata and saves.
   Scripts/RunInEditor.py works when its unicast port reaches the intended editor. If another
   Unreal process owns that port, use Unreal's official remote_execution client with multicast
   discovery and select this project's node instead.
4. Run python3 Scripts/GenerateDialogueVoices.py --check and
   AshesOfHeaven.LevelOne.DialogueVoiceCoverage. Rebuild/restart only for C++ changes.

Model: [Qwen3-TTS VoiceDesign](https://huggingface.co/mlx-community/Qwen3-TTS-12Hz-1.7B-VoiceDesign-8bit).
Runtime: [MLX Audio](https://github.com/Blaizzy/mlx-audio).
Character descriptions design fictional voices; no real person's voice was cloned.

The generator preserves the approved child source instead of resynthesizing it. Its SHA-256
is recorded in the manifest. Keep SourceAudio under version control alongside the game assets.
