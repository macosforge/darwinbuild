---
title: Subversion Support
slug: subversion-support
date: 2009-08-09 13:54:55.86564-07
---

Projects can now have a branch key in their plist which will cause darwinbuild to use subversion to checkout/update the source instead of downloading a tarball. You can see an [example](https://smartcardservices.macosforge.org/trac/browser/trunk/SmartcardCCID/SmartcardCCID.plist) of this at the SmartCard Services project. Darwinbuild will assume `-nosource` when a project is subversion-based. The working copy is in the same place as usual, at BuildRoot/SourceCache/project/project-version/. This should allow developers to take advantage of the chroot and automatic root fetching of darwinbuild, while being able to make changes between builds and commit directly from the SourceCache.
