---
title: Fetching All Source For a Release
---

If you want to download all of the source for a release, here is a simple procedure using just a shell and darwinbuild (using 9G55 as an example):

1. [install darwinbuild](../install/)
2. `mkdir 9G55 && cd 9G55`
3. `sudo -s`
4. `darwinbuild -init 9G55`
5. `for X in $(darwinxref version '*' | cut -d '-' -f 1); do darwinbuild -fetch $X; done`
6. `mkdir AllSource`
7. `for X in Sources/*; do tar zxvf $X -C AllSource/; done`
