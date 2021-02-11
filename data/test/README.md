ffmpeg commands:

./ffmpeg -f dshow -channels 1 -t 5 -i audio="Microphone (HK 1080P Cam)" -f f32be -y "audio.raw"

./ffplay -channels 1 -f f32be -i audio.raw
