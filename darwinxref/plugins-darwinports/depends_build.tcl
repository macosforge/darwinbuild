DBPluginSetName depends_build
DBPluginSetType property
DBPluginSetDatatype array

proc usage {} {
	return {[<project>]}
}

proc run {args} {
	set project [lindex $args 0]
	set build [DBGetCurrentBuild]
	foreach dep [DBCopyPropArray $build $project depends_build] {
		puts "$dep"
	}
}
