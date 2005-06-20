DBPluginSetName description
DBPluginSetType property
DBPluginSetDatatype string

proc usage {} {
	return {[<project>]}
}

proc run {args} {
	set project [lindex $args 0]
	set build [DBGetCurrentBuild]
	puts [DBCopyPropString $build $project description]
}
