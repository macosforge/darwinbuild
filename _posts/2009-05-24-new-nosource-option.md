---
title: New -nosource Option
slug: new-nosource-option
permalink: /post/new-nosource-option
date: 2009-05-24 18:04:08.121527-07
---

We have added in an option that many have been asking for. You can now pass `-nosource` to darwinbuild in order to skip the source staging. This means darwinbuild will not delete the BuildRoot/SourceCache/ files and replace them with the tarball. This option also disables file patching, since presumably the patches were already applied when the tarball was extracted. So, now you can modify the SourceCache as needed without darwinbuild deleting your changes.
