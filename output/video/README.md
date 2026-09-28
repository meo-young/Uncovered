# 휴대폰 메시지 영상

- 결과: `Phone_Messages_Dark.mp4`
- 규격: 888 × 1776, H.264 MP4, 30 fps, 5초, 무음
- 0초 이상 1초 미만: 마지막 메시지로 `어디야 ?`를 표시합니다.
- 1초 이상 3초 미만: 점 세 개의 높이와 밝기를 순차적으로 변경합니다.
- 3초 이상 5초 미만: 입력 표시를 제거하고 `올 때 커피좀 !`을 표시합니다.
- 기존 다크 테마 이미지와 내장 image_gen으로 만든 메시지 도착 전 배경을 FFmpeg로 합성합니다.
- 재생성: imageio-ffmpeg가 설치된 Python으로 `render_phone_video.py`를 실행합니다.

## 배경 편집 프롬프트

Edit only the bottommost incoming message bubble containing "올 때 커피좀 !" in the provided dark phone chat screenshot: completely remove that one bubble and its text, seamlessly fill its former area with the surrounding near-black charcoal chat background. The last visible message must now be "어디야 ?". Preserve absolutely everything else pixel-aligned: all previous five message bubbles and text, name 김준삽, 08:32, LTE, half battery, landscape avatars, date divider, empty composer, 1:2 aspect ratio. No shifts, no new elements, no typing bubble, no dots. Flat screen texture only.
