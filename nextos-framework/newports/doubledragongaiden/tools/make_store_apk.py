#!/usr/bin/env python3
"""Build the store-only game.apk Unity addresses as a JAR (Double Dragon Gaiden).

libunity opens assets through `jar:file://<path>!/assets/...`, and a directory
cannot be opened as a zip, so the assets tree ships also as a store zip whose
entries are exactly the APK-relative paths Unity asks for.  Entries are stored
uncompressed so the engine can seek straight into them.

LocalVideos is excluded here for the first batch, mirroring deploy_game.py;
pass --with-videos to include it once the video phase starts.

Usage: make_store_apk.py [--with-videos]
"""
import os
import sys
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
PORT = os.path.dirname(HERE)
ASSETS = os.path.join(PORT, "apk", "assets")
OUT = os.path.join(PORT, "game.apk")


def main():
    with_videos = "--with-videos" in sys.argv
    if os.path.exists(OUT):
        os.replace(OUT, OUT + ".prev")
    total = 0
    written = 0
    with zipfile.ZipFile(OUT, "w", zipfile.ZIP_STORED, allowZip64=True) as z:
        for root, dirs, files in os.walk(ASSETS):
            rel = os.path.relpath(root, ASSETS)
            if not with_videos and (rel == "LocalVideos" or
                                    rel.startswith("LocalVideos" + os.sep)):
                dirs[:] = []
                continue
            for name in sorted(files):
                src = os.path.join(root, name)
                arc = "assets/" + (name if rel == "." else
                                   os.path.relpath(src, ASSETS).replace(os.sep, "/"))
                z.write(src, arc)
                total += os.path.getsize(src)
                written += 1
    print("wrote %s: %d entries, %.1f MB (stored)"
          % (OUT, written, total / 1048576.0))


if __name__ == "__main__":
    main()
