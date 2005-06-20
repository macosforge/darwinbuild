DBPluginSetName platforms
DBPluginSetType property
DBPluginSetDatatype array

proc usage {} {
	return {[<project>]}
}

proc run {args} {
	set project [lindex $args 0]
	set build [DBGetCurrentBuild]
	foreach plat [DBCopyPropArray $build $project platforms] {
		puts "$plat"
	}
}
