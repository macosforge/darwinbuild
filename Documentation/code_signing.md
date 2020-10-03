# Code Signing with darwinbuild

darwinbuild supports Apple code signing, both automatically via Xcode,
and manually from the command line. Code signing is enabled by passing
the `-codesign` flag on the command line, like this:

```shell
sudo darwinbuild -codesign='Developer ID Application' some_project
```

The string after the equals sign is the name of the certificate you want
Xcode to use. However, since darwinbuild must be run as root, it does not
have access to your personal keychain (which is where I presume you are storing
your signing materials). Therefore, you **must** run the steps under “Keychain
Preparation” below at least once before this will work, and ensure that the
DEVELOPER_TEAM property is set every time you build. You can either set this in
your project file, or via the `environment` key in the darwinbuild plist.
Passing the variable via `export` or `sudo env` will *not* work.

## Format of darwinbuild-codesign.plist

If you want to do manual code-signing (if your project is not built by Xcode,
for example), install a file called `darwinbuild-codesign.plist` in the
`/usr/local/darwinbuild` folder of your DSTROOT during the build. After the
build is complete, darwinbuild will locate the file, parse it, perform
the signing, and then delete it from the DSTROOT (so the codesign plists
do not collide, which would confuse darwinbuild no end).

Here is a sample `darwinbuild-codesign.plist` file:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>hardened_runtime</key>
	<true/>
	<key>prefix</key>
	<string>com.mycompany.</string>
	<key>timestamp</key>
	<true/>
	<key>files</key>
	<dict>
		<key>/usr/local/bin/my_library</key>
		<true/>
		<key>/usr/local/lib/my_program</key>
		<dict>
			<key>identifier</key>
			<string>com.mycompany.my_special_program</string>
			<key>order</key>
			<string>1</string>
		</dict>
	</dict>
</dict>
</plist>
```

At the top level are keys that provide settings for all files. These are:

* `hardened_runtime`: Boolean, default false. If true, `-o runtime` will be included in the codesign arguments.
* `prefix`: String, corresponds to the `--prefix` flag of codesign. If you provide a value, it must end in a `.`, or codesign will not create the signing identifier you expect (it won’t add the dot for you).
* `timestamp`: Either boolean or string. If `true` (default), use Apple’s timestamping service. If `false`, no timestamp server will be used. If a string, it will be interpreted as the URL of the timestamping server to use.

The `files` dictionary contains as keys list of files to sign. These are usually
provided as absolute paths, but are treated relative to the DSTROOT. If the
value of the key is `true`, all default settings (at the top level of the plist)
will be used. Otherwise, it is interpreted as a dictionary with the following keys:

* `identifier`: The code signing identifier. It will be used verbatim by codesign. Per the manual page, do **NOT** sign two different pieces of code with the same identifier.
* `hardened_runtime`: Overrides the default `hardened_runtime` setting, if present.
* `prefix`: Overrides the default `prefix` setting, if present.
* `dr`: The Designated Requirement for the code being signed. This is usually computed automatically during the sgning process.
* `order`: The order index at which the file will be signed.

Every file in the `files` dictionary has an order index. By default, all files
have an order index of 65535. If you override this setting with another value,
you can control in which order the files are signed. This can be useful when
signing bundles that contain other bundles that also need to be signed.
Files with the lowest index values are signed first.

## Keychain Preparation

By default, darwinbuild does not have access to your user keychain, because it
runs xcodebuild in a different user session than the sudo process provides. You
must therefore add the keychain with your signing materials (referred to below
as `$KEYCHAIN_PATH`). Follow the below steps.

1. `sudo su`; enter password
2. `security list-keychains -d user -s $KEYCHAIN_PATH /Library/Keychains/System.keychain`
3. `security unlock-keychain $KEYCHAIN_PATH`; enter password
4. `security list-keychains`; if `$KEYCHAIN_PATH` is listed in the output, you’re good to go.

This should only need to be done once per Mac. Note that there is a possibility
that macOS updates will undo this change.  If this ever happens, simply repeat the above steps.
