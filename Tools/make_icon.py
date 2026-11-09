"""Builds the Pac Shark app icons: the shark's head from the main menu art.

Usage: python make_icon.py <project folder> [main menu image]
The main menu image defaults to <project>/Art/Main_Menu.png (the full-size original)
if it is there, otherwise <project>/Art/MainMenu.png. The original is sharpest.
Writes Build/Windows/Application.ico, the Android icons under Build/Android/res,
and Art/Icon_512.png (store listing). Needs only Pillow (pip install pillow).
"""
import os
import sys
from PIL import Image, ImageDraw, ImageStat

ROOT = sys.argv[1]
MENU = sys.argv[2] if len(sys.argv) > 2 else os.path.join(ROOT, 'Art', 'Main_Menu.png')
if not os.path.exists(MENU):
    MENU = os.path.join(ROOT, 'Art', 'MainMenu.png')

# Square around the shark's head, as fractions of the main menu image width.
HEAD_BOX = (300 / 2725, 220 / 2725, 940 / 2725, 860 / 2725)

ADAPTIVE_XML = """<?xml version="1.0" encoding="utf-8"?>
<adaptive-icon xmlns:android="http://schemas.android.com/apk/res/android">
    <background android:drawable="@drawable/icon_background" />
    <foreground android:drawable="@drawable/icon_foreground" />
</adaptive-icon>
"""


def head_square():
    im = Image.open(MENU).convert('RGB')
    w = im.width
    return im.crop(tuple(round(v * w) for v in HEAD_BOX))


def rounded(img, n):
    out = img.resize((n, n), Image.LANCZOS).convert('RGBA')
    # Draw the mask at 4x and shrink it for smooth corners.
    m = Image.new('L', (n * 4, n * 4), 0)
    ImageDraw.Draw(m).rounded_rectangle([0, 0, n * 4 - 1, n * 4 - 1], radius=int(n * 4 * 0.22), fill=255)
    out.putalpha(m.resize((n, n), Image.LANCZOS))
    return out


def save(img, *parts):
    p = os.path.join(ROOT, *parts)
    os.makedirs(os.path.dirname(p), exist_ok=True)
    img.save(p)
    print(p)


if __name__ == '__main__':
    head = head_square()
    print('source', MENU, 'head crop', head.size)

    # Windows: multi-size .ico, each size downscaled straight from the source.
    p = os.path.join(ROOT, 'Build', 'Windows', 'Application.ico')
    os.makedirs(os.path.dirname(p), exist_ok=True)
    sizes = [16, 24, 32, 48, 64, 128, 256]
    rounded(head, 256).save(p, format='ICO', sizes=[(s, s) for s in sizes],
                            append_images=[rounded(head, s) for s in sizes[:-1]])
    print(p)

    # Android legacy icons (used below Android 8).
    for d, n in [('drawable', 48), ('drawable-ldpi', 36), ('drawable-mdpi', 48), ('drawable-hdpi', 72),
                 ('drawable-xhdpi', 96), ('drawable-xxhdpi', 144), ('drawable-xxxhdpi', 192)]:
        save(rounded(head, n), 'Build', 'Android', 'res', d, 'icon.png')

    # Android adaptive icon (Android 8+): 108dp layers. The head fills the 72dp
    # visible area in the middle; the background is the water colour around it.
    water = tuple(int(v) for v in ImageStat.Stat(head.crop((0, 0, head.width, head.height // 8))).mean)
    for d, n in [('drawable-mdpi', 108), ('drawable-hdpi', 162), ('drawable-xhdpi', 216),
                 ('drawable-xxhdpi', 324), ('drawable-xxxhdpi', 432)]:
        save(Image.new('RGB', (n, n), water), 'Build', 'Android', 'res', d, 'icon_background.png')
        inner = round(n * 72 / 108)
        fg = Image.new('RGBA', (n, n), (0, 0, 0, 0))
        fg.paste(head.resize((inner, inner), Image.LANCZOS), ((n - inner) // 2, (n - inner) // 2))
        save(fg, 'Build', 'Android', 'res', d, 'icon_foreground.png')
    p = os.path.join(ROOT, 'Build', 'Android', 'res', 'drawable-anydpi-v26', 'icon.xml')
    os.makedirs(os.path.dirname(p), exist_ok=True)
    with open(p, 'w') as f:
        f.write(ADAPTIVE_XML)
    print(p)

    # Store listing icon (square: the store rounds it).
    save(head.resize((512, 512), Image.LANCZOS), 'Art', 'Icon_512.png')
