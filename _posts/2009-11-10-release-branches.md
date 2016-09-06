---
title: Release Branches
slug: release-branches
date: 2009-11-10 08:02:42.363389-08
---

In order to make it clear what is expected to work on a given OS release, we have split the source into release branches. If you are using darwinbuild on Tiger (Mac OS X 10.4.x, Darwin 8), use [/releases/Darwin8](https://svn.macosforge.org/repository/darwinbuild/releases/Darwin8/). If you are on Leopard (Mac OS X 10.5.x, Darwin 9) use [/releases/Darwin9](https://svn.macosforge.org/repository/darwinbuild/releases/Darwin9/). [Trunk](https://svn.macosforge.org/repository/darwinbuild/trunk/) is currently intended for Snow Leopard (Mac OS X 10.6.x, Darwin 10).

We'll backport any critical changes and features that do not rely on the newer release to the release branches. This should hopefully fix the problem of hunting for the last revision that worked on Tiger, for example.
