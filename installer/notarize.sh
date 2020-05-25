#!/bin/sh

MY_DIR=$(cd `dirname $0` && pwd)
cd $MY_DIR

if [ ! -f $MY_DIR/darwinbuild-installer.pkg ]; then
	echo "darwinbuild-installer.pkg not present, please run build.sh to create it" 1>&2
	exit 1
fi

ALTOOL_LOG=$(mktemp -q -t altool.log)
if [ $? -ne 0 ]; then
	echo "Could not create temporary file for altool log" 1>&2
	exit 1
fi

xcrun altool --notarize-app --primary-bundle-id org.puredarwin.darwinbuild.release \
	--username "wjk011@gmail.com" --password "@keychain:ADC Notarization" \
	--file $MY_DIR/darwinbuild-installer.pkg \
	--output-format xml > $ALTOOL_LOG
if [ $? -ne 0 ]; then
	echo "altool failed to upload, cannot continue" 1>&2
	echo "altool log file is located at: $ALTOOL_LOG" 1>&2
	exit 1
fi

request_id=$(/usr/libexec/PlistBuddy -c "print :notarization-upload:RequestUUID" $ALTOOL_LOG)
if [[ $request_id =~ ^\{?[A-F0-9a-f]{8}-[A-F0-9a-f]{4}-[A-F0-9a-f]{4}-[A-F0-9a-f]{4}-[A-F0-9a-f]{12}\}?$ ]]; then

	while :; do
		echo "Sleeping 20 seconds before checking notarization status..."
		sleep 20

		xcrun altool --notarization-info $request_id \
			--username "wjk011@gmail.com" --password "@keychain:ADC Notarization" \
			--output-format xml > $ALTOOL_LOG

		notarization_status=$(/usr/libexec/PlistBuddy -c "print :notarization-info:Status" $ALTOOL_LOG)

		if [ ! -z "$notarization_status" ]; then
			[ "$notarization_status" != "in progress" ] && break
		fi
	done

	log_url=$(/usr/libexec/PlistBuddy -c "print :notarization-info:LogFileURL" $ALTOOL_LOG)
	echo "Log file can be downloaded from: $log_url"

	if [ "$notarization_status" != "success" ]; then
		echo "altool reported notarization error" 1>&2
		exit 1
	fi

	xcrun stapler staple $MY_DIR/darwinbuild-installer.pkg
	xcrun stapler validate -v $MY_DIR/darwinbuild-installer.pkg
	if [ $? -ne 0 ]; then
		echo "validation of installer notarization failed" 1>&2
		exit 1
	fi
else
	echo "Invalid request ID found in altool output, cannot continue." 1>&2
	exit 1
fi
