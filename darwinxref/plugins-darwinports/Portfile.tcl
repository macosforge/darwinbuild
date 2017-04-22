DBPluginSetName Portfile
DBPluginSetType property.project
DBPluginSetDatatype data

proc run {args} {
	set project [lindex $args 0]
	set build [DBGetCurrentBuild]
	fconfigure stdout -encoding binary -translation binary
	puts -nonewline [DBCopyPropData $build $project Portfile]
}
