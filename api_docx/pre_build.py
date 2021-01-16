#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os

ROOT_PATH = os.path.abspath( os.path.join(os.path.dirname( __file__ ), ".."))


files = os.listdir(os.path.join(ROOT_PATH, "doc"))

index = []
for fname in sorted(files):
    if not fname.endswith(".txt"):
        continue
    cmd_name = fname[:-4]
    with open(os.path.join(ROOT_PATH, "doc", fname), "r") as f:
        contents = f.read()
        f.close()

    out_file = os.path.join(ROOT_PATH, "temp-man-pages", "%s.md" % cmd_name)
    with open(out_file, "w") as f:
        s = "# %s {#%s}\n\n```\n%s\n```\n" % (cmd_name, cmd_name, contents)
        f.write(s)
        f.close()
        index.append(cmd_name)
    print(" wrote %s" % out_file)

out_file = os.path.join(ROOT_PATH, "temp-man-pages", "index.md")

s =  "Man Pages {#man-pages}\n"
s += "=========================\n\n"
for page in index:
    s += "- @subpage %s\n" % page
s += "\n"

with open(out_file, "w") as f:
    f.write(s)
    f.close()
