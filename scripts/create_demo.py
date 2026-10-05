#!/usr/bin/env python3
"""Create a disposable demo without replacing existing files."""
from pathlib import Path
root = Path(__file__).resolve().parent.parent / "test-playground"
files = {
    "01-welcome.md": "# A quiet place for your files\n\nWelcome to TermNav.\n\n  h j k l   Move through your filesystem\n  /         Find something\n  Space     Mark a file\n  y then p  Copy between directories\n  ?         Discover every shortcut\n\nExplore the folders. Make something. Move freely.\n",
    "projects/aurora/README.md": "# Aurora\n\nA small idea, becoming something good.\n\n## Next steps\n- Design the experience\n- Build with care\n- Ship when it is ready\n",
    "projects/aurora/main.c": '#include <stdio.h>\n\n// Every good project starts somewhere.\nint main(void) {\n    puts("Hello, Aurora.");\n    return 0;\n}\n',
    "notes/field-notes.md": "# Field notes\n\nLess noise. More intention.\n\nKeep your tools small and your ideas big.\n",
    "notes/日本語.txt": "こんにちは、TermNav。\nUnicode filenames and content belong here too.\n",
    "documents/brief.txt": "PROJECT BRIEF\n\nA keyboard-first home for real files.\nBuilt in C. Powered by the operating system.\n",
    ".hidden-note": "Press . to reveal the things just out of sight.\n",
    "palette.json": '{\n  "background": "#1c1c1c",\n  "accent": "#d7af87",\n  "directories": "#87afd7",\n  "intent": "quiet, useful, considered"\n}\n',
}
for name, data in files.items():
    path = root / name
    path.parent.mkdir(parents=True, exist_ok=True)
    if not path.exists():
        path.write_text(data, encoding="utf-8")
        path.chmod(0o644)
(root / "inbox").mkdir(exist_ok=True)
binary = root / "sample.bin"
if not binary.exists():
    binary.write_bytes(bytes(range(256)))
    binary.chmod(0o644)
print(f"Demo ready: {root}")
try:
    from PIL import Image, ImageDraw
except ImportError:
    pass
else:
    image_path=root / "landscape.png"
    if not image_path.exists():
        image=Image.new("RGB",(600,360),(41,58,77)); draw=ImageDraw.Draw(image)
        draw.ellipse((380,50,440,110),fill=(234,184,122))
        draw.polygon([(0,270),(170,100),(330,290),(465,140),(600,270),(600,360),(0,360)],fill=(94,120,129))
        draw.polygon([(0,320),(250,190),(420,330),(600,240),(600,360),(0,360)],fill=(35,76,82))
        image.save(image_path)
    import zipfile
    archive_path=root / "project.zip"
    if not archive_path.exists():
        with zipfile.ZipFile(archive_path,"w") as archive:
            archive.writestr("aurora/README.md","# Aurora\nA quiet place for your ideas.\n")
            archive.writestr("aurora/src/main.c",'int main(void) { return 0; }\n')
