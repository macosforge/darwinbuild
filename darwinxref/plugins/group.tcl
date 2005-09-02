DBPluginSetName group
DBPluginSetType basic

proc usage {} {
	return {<group>}
}

proc run {args} {
	set group [lindex $args 0]
	if {$group == ""} { return -1 }
	foreach member [DBCopyGroupMembers [DBGetCurrentBuild] $group] {
		puts $member
	}
}
