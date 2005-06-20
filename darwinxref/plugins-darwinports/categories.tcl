DBPluginSetName categories
DBPluginSetType property
DBPluginSetDatatype array

proc usage {} {
	return {[<project>]}
}

proc run {args} {
	set project [lindex $args 0]
	set build [DBGetCurrentBuild]
	foreach cat [DBCopyPropArray $build $project categories] {
		puts "$cat"
	}
}
