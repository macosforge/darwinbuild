---
title: New Storage Options
slug: new-storage-options
date: 2009-04-17 18:54:52.540124-07
---

The latest revision of trunk has support for sparsebundles and NFS Loopback in order to avoid the problems with xcodebuild inside of chroots. If you do *not* change the way you use darwinbuild, you will start seeing the sparsebundle storage. Nothing else is needed and Xcode-based projects will build on whatever filesystem you have.

<!--more-->

You can optionally use NFS to mount the buildroot which also works around the xcodebuild/chroot problems. This method is Leopard-only and will cause lines to be added to /etc/exports. Since there is no concept of "uninitializing" a build environment, the lines added to /etc/exports are never automatically removed. You may need to cleanup /etc/exports from time to time. You setup the NFS storage by passing `-nfs` after your `-init` command:

    darwinbuild -init 9G55 -nfs

If you want the old legacy directory mode for whatever reason, you can initialize your build environment with -nodmg:

    darwinbuild -init 9G55 -nodmg
