# Creating Build Definition Property Lists

To use darwinbuild, you will need to create an XML-format property list that
contains the build definitions. A barebones example looks like this:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>source_sites</key>
	<array>
	</array>

	<key>build</key>
	<string>PD17D4</string>

	<key>environment</key>
	<dict>
		<key>MACOSX_DEPLOYMENT_TARGET</key>
		<string>10.13</string>
		<key>SDKROOT</key>
		<string>macosx10.13</string>
	</dict>

	<key>projects</key>
	<dict>
		<key>xnubuild</key>
		<dict>
			<key>version</key>
			<string>10.13</string>
		</dict>
	</dict>
</dict>
</plist>
```

Here is a key-by-key explanation of the above property list:

* **`source_sites`**  
  This is a list of URLs from which darwinbuild will attempt to download the
  source code for the projects it will build. Each URL will be tried in the order
  it is specified here. The desired filename will be appended to each URL. If
  the download returns a 404 or other error, the next URL will be tried.

  The only exception to the above logic is in the case of `opensource.apple.com`.
  If the URL to download from contains the substring `opensource.apple.com/tarballs`,
  then the project name (the part of the filename to download before the first hyphen)
  will be prepended before the filename, like this: `opensource.apple.com/tarballs/<project>/<filename>`.
  This is done to align with the way Apple organizes their website.
* **`build`**  
  This is the identifier of the “build” of PureDarwin the property list represents.
  While there is no requirements for its format, it *must* match the filename exactly
  (minus the `.plist` extension). Otherwise, darwinbuild will be unable to correctly
  load the build definitions.
* **`environment`**  
  This dictionary contains a 1:1 mapping of environment variables that will
  be propagated into each build command invoked by darwinbuild.
* **`projects`**  
  This is where the meat of the build definition is. The contents of
  this dictionary will be described under “Project Definitions,” below.

### Project Definitions

A “project” is the core of the darwinbuild dependency model. The name of the project
is specified as the key in the `projects` dictionary. A project’s name is used to
refer to the project from both the command line, and from other projects as dependencies.

The “version” of a project is in practice an arbitrary string, and is specified as
the `version` key in the project definition. Apple open-source releases
use a dot-separated sequence of one to five positive numbers here, but the logic behind
the changing of these numbers has never been publicly described. There is no strict
interpretation of which version is “newer” than another; if the version number has changed
in any way, the the project sources have changed and must be rebuilt. darwinbuild supports
keeping multiple versions of the same project in the Sources subdirectory of the build directory.

The version does, however, determine the name of the source tarball to be downloaded.
It will always have the following format: `<project_name>-<version>.tar.gz`. The source
tarball must be available from one of the `source_sites` under this filename. If none
of the `source_sites` contain a copy of this file, darwinbuild also supports downloading
source archives from GitHub Releases; this is detailed in the “GitHub Releases Support” section.
