---
title: New -init Options
slug: new-init-options
permalink: /post/new-init-options
date: 2009-07-31 23:55:28.536554-07
---

We have added some new logic to the `-init` processing that will look for http or scp URLs/paths and download the plist automatically. If you have been storing your own plist on a server and making users manually download the plist, you can now provide a URL and let darwinbuild download it for you:

    darwinbuild -init http://example.com/files/9J61plus.plist

or using scp...

    darwinbuild -init user@example.com:/files/9J61plus.plist
