from pathlib import Path
import subprocess
import imageio_ffmpeg

ROOT = Path(__file__).resolve().parent
FFMPEG = imageio_ffmpeg.get_ffmpeg_exe()
FINAL = ROOT.parent / 'imagegen' / 'T_Phone_Messages_KimJunsap_Dark.png'
CLEAN = ROOT / 'Phone_Dark_Before_Coffee.png'
OUTPUT = ROOT / 'Phone_Messages_Dark.mp4'

# 합성 영역을 마지막 말풍선에 한정하여 기존 화면의 흔들림을 방지합니다.
feather = '255*min(1,min(min(X,W-1-X),min(Y,H-1-Y))/8)'
filters = [
    '[0:v]format=rgba[original]',
    f"[1:v]crop=340:124:138:914,format=rgba,geq=r='r(X,Y)':g='g(X,Y)':b='b(X,Y)':a='{feather}'[clean]",
    "[original][clean]overlay=138:914:enable='lt(t,3)'[base]",
]

# 점 세 개를 순서대로 위아래로 움직이고 밝기를 변화시킵니다.
channels = []
for base in (43, 48, 56):
    expression = str(base)
    for i, x in reversed(list(enumerate((44, 80, 116)))):
        pulse = f'max(0,sin(2*PI*(T-1-{i}*0.15)/0.85))'
        inside = f'lte(pow(X-{x},2)+pow(Y-(47-7*({pulse})),2),49)'
        expression = f'if({inside},155+80*({pulse}),{expression})'
    channels.append(expression)
rounded = '255*lte(pow(max(abs(X-80)-60,0),2)+pow(max(abs(Y-44)-24,0),2),400)'
filters.append(
    "color=c=black:s=160x88:r=30:d=5,format=rgba,"
    f"geq=r='{channels[0]}':g='{channels[1]}':b='{channels[2]}':a='{rounded}'[typing]"
)
filters += [
    "[base][typing]overlay=150:928:enable='gte(t,1)*lt(t,3)'[animated]",
    '[animated]scale=888:1776:flags=lanczos,setsar=1,format=yuv420p[out]',
]
graph = ';\n'.join(filters)
(ROOT / 'phone_animation.ffscript').write_text(graph, encoding='utf-8')
subprocess.run([
    FFMPEG, '-y', '-hide_banner', '-loglevel', 'warning',
    '-loop', '1', '-framerate', '30', '-i', str(FINAL),
    '-loop', '1', '-framerate', '30', '-i', str(CLEAN),
    '-filter_complex_script', str(ROOT / 'phone_animation.ffscript'),
    '-map', '[out]', '-t', '5', '-r', '30', '-an',
    '-c:v', 'libx264', '-preset', 'slow', '-crf', '16',
    '-movflags', '+faststart', str(OUTPUT),
], check=True)

# 주요 시점의 실제 인코딩 프레임을 추출하여 결과를 검토합니다.
for label, moment in [('start', 0), ('typing_a', 1.2), ('typing_b', 1.5), ('before_arrival', 2.966667), ('arrival', 3.0)]:
    subprocess.run([
        FFMPEG, '-y', '-hide_banner', '-loglevel', 'error',
        '-ss', str(moment), '-i', str(OUTPUT), '-frames:v', '1',
        '-update', '1', str(ROOT / f'check_{label}.png'),
    ], check=True)
print(OUTPUT)
