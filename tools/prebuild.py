# PlatformIO ön derleme: gömülü UI başlığını kaynaktan üretir (değişmediyse dosyaya dokunmaz).
Import("env")  # noqa: F821
import subprocess, sys, os
root = env["PROJECT_DIR"]  # noqa: F821
if env.get("PIOENV") != "native":  # noqa: F821
    subprocess.check_call([sys.executable, os.path.join(root, "tools", "ui", "assemble.py")])
