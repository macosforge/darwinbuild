DBPluginSetName currentBuild
DBPluginSetType basic

proc usage {} {
	return {}
}

proc run {args} {
	puts [DBGetCurrentBuild]
}
