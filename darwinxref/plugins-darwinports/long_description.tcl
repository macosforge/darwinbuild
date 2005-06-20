DBPluginSetName long_description
DBPluginSetType property
DBPluginSetDatatype string

proc usage {} {
	return {[<project>]}
}

proc run {args} {
	set project [lindex $args 0]
	set build [DBGetCurrentBuild]
	puts [DBCopyPropArray $build $project long_description]
}
