---
title: Install Darwinbuild
---

## Installing Darwinbuild from Source

### Darwin 10 and 11 (Snow Leopard/Lion)

1. `cd /tmp`
2. `git clone https://github.com/macosforge/darwinbuild.git darwinbuild`
3. `cd darwinbuild`
4. `sudo xcodebuild install DSTROOT=/`

### Darwin 9 (Leopard)

1. `cd /tmp`
2. `git clone -b Darwin9 https://github.com/macosforge/darwinbuild.git darwinbuild`
3. `cd darwinbuild`
4. `make`
5. `sudo make install`

### Darwin 8 (Tiger)

1. `cd /tmp`
2. `git clone -b Darwin8 https://github.com/macosforge/darwinbuild.git darwinbuild`
3. `cd darwinbuild`
4. `make`
5. `sudo make install`

## Installing via MacPorts

If you already use [MacPorts](https://www.macports.org/) for other software, you might find it easier to install Darwinbuild from there.

### Darwin 10 (Snow Leopard)

1. If you already have the sqlite3 and openssl ports installed, make sure they are installed with the universal variant.
2. `sudo port install darwinbuild +universal`

### Darwin 8 and 9

1. `sudo port install darwinbuild-legacy`

### Upgrading

If you had the darwinbuild port installed on Darwin 8 or 9, then `sudo port upgrade darwinbuild` should trigger the replacement process to give you darwinbuild-legacy.
