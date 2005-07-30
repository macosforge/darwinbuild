DBPluginSetName binary_sites
DBPluginSetType property
DBPluginSetDatatype array

proc usage {} {
	return {[<project>]}
}

proc run {args} {
	set project [lindex $args 0]
	set build [DBGetCurrentBuild]
	set sites [DBCopyPropArray $build $project binary_sites]
	if {$project != "" && [llength $sites] == 0} {
		# use build-wide settings if project settings not found
		set sites [DBCopyPropArray $build "" binary_sites]
	}
	foreach site $sites {
		puts "$site"
	}
}
