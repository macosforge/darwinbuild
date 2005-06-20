DBPluginSetName maintainers
DBPluginSetType property
DBPluginSetDatatype array

proc usage {} {
	return {[<project>]}
}

proc run {args} {
	set project [lindex $args 0]
	set build [DBGetCurrentBuild]
	foreach maint [DBCopyPropArray $build $project maintainers] {
		puts "$maint"
	}
}
