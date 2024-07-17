#!/usr/bin/perl

# process-logs.pl
# Kevin Van Vechten <kvv@apple.com>

use strict;
use File::Basename;
use File::Glob ':glob';

my $darwinxref = '/usr/local/bin/darwinxref';

sub getBuild {
	local *BUILD;
	my $path = '.build/build';
	if ($ENV{'DARWIN_BUILDROOT'}) {
		$path = $ENV{'DARWIN_BUILDROOT'} . "/$path";
	}
	open BUILD, $path || return undef;
	my $build = <BUILD>;
	chomp $build;
	close BUILD;
	return $build;
}

sub getBuildVersion {
	my $maxbuild = 0;
	foreach (@_) {
		m/~([0-9]+)$/;
		$maxbuild = $1 if $1 > $maxbuild;
	}
	return $maxbuild;
}

sub getProjects {
	my $build = shift;

	local *PROJECTS;
	open PROJECTS, '-|', $darwinxref, '-b', $build, 'version', '?' || die;
	my @projects;
	while (<PROJECTS>) {
		my $project = $_;
		chomp($project);
		push @projects, $project;
	}
	close PROJECTS;

	return @projects;
}

sub getTitle {
	my $build = shift;
	my $result = "";
	local *STREAM;
	open STREAM, '-|', $darwinxref, '-b', $build, 'darwin' || die;
	while(<STREAM>) { $result .= $_; }
	close STREAM;
	$result .= " (";
	open STREAM, '-|', $darwinxref, '-b', $build, 'macosx' || die;
	while(<STREAM>) { $result .= $_; }
	close STREAM;
	$result .= ")";
	return $result;
}

###
### Main
###

my $BUILD = $ARGV[0] ? $ARGV[0] : &getBuild();

if (!$BUILD) {
	print STDERR <<EOB;
ERROR: please change your working directory to one initialized by:
  darwinbuild -init <build>
Alternatively, you may set the DARWIN_BUILDROOT environment variable to the
absolute path of that directory.
EOB
	exit 1;
}

my $TITLE = &getTitle($BUILD);

my $LOGDIR;
if ($ENV{'DARWIN_BUILDROOT'}) {
	$LOGDIR = $ENV{'DARWIN_BUILDROOT'} . "/Logs";
} else {
	$LOGDIR = "Logs";
}

print <<EOB;
<html>
<head>
<title>$BUILD</title>

<script>
function sortTable (table, compare) {
  //
  // create an array from the table in the html section
  //
  var rows = new Array();
  var len = table.tBodies[0].rows.length;
  for (var i = 0; i < len; i++) {
	rows[i] = table.tBodies[0].rows[i].cloneNode(true);
  }

  // clear out the table
  for (var i = 0; i < len; i++) {
	table.tBodies[0].deleteRow(0);
  }

  // sort the array
  rows.sort(compare);

  // replace the table contents
  for (var i = 0; i < rows.length; i++) {
    table.tBodies[0].appendChild(rows[i]);
  }
}

function sortByProject(row1, row2) {
	// XXX 0 == Project column
	// dereference twice, since there is a link element
  var a = row1.cells[0].firstChild.firstChild.nodeValue;
  var b = row2.cells[0].firstChild.firstChild.nodeValue;
  return (a < b) ? - 1 : (a == b ? 0 : 1);
}

function sortByAction(row1, row2) {
	// XXX 1 == Action column
	// dereference twice, since there is a small element
  var a = row1.cells[1].firstChild.firstChild.nodeValue;
  var b = row2.cells[1].firstChild.firstChild.nodeValue;
  return (a > b) ? - 1 : (a == b ? sortByProject(row1, row2) : 1);
}

function sortByArchs(row1, row2) {
	// XXX 2 == Archs column
	// dereference twice, since there is a small element
  var a = row1.cells[2].firstChild.firstChild.nodeValue;
  var b = row2.cells[2].firstChild.firstChild.nodeValue;
  return (a > b) ? - 1 : (a == b ? sortByProject(row1, row2) : 1);
}

function sortByExitStatus(row1, row2) {
	// XXX 3 == Exit Status column
  var a = parseFloat(row1.cells[3].firstChild.nodeValue);
  var b = parseFloat(row2.cells[3].firstChild.nodeValue);
  return (a > b) ? - 1 : (a == b ? sortByProject(row1, row2) : 1);
}

</script>


</head>

<body>
<h2>$BUILD</h2>
<h3>$TITLE</h3>
<p>Build log summary.</p>

<table id="logtab">
<thead>
<tr>
  <th align="left"><a href="#" onclick="sortTable(document.getElementById('logtab'), sortByProject)">Project</a></th>
  <th align="left"><a href="#" onclick="sortTable(document.getElementById('logtab'), sortByAction)">Action</a></th>
  <th align="left"><a href="#" onclick="sortTable(document.getElementById('logtab'), sortByArchs)">Archs</a></th>
  <th align="left"><a href="#" onclick="sortTable(document.getElementById('logtab'), sortByExitStatus)">Exit Status</a></th>
</tr>
</thead>

<tbody>
EOB

foreach my $project (getProjects($BUILD)) {
	my $projnam = $project;
	$projnam =~ s/-(.*)$//;

	my $build_version = "";
	my $buildaction = "";
#	my $chrooted = "";
	my $exitstatus = "";
	my $rcarchs = "";
	my $color = "";
	my $logfile;
	
	if ( -e "$LOGDIR/$projnam" ) {
		$build_version = getBuildVersion(bsd_glob("$LOGDIR/$projnam/$project.*"));
		$logfile = "$LOGDIR/$projnam/$project.log~$build_version";
		if ( -e $logfile ) {
			local *LOG;
			open LOG, $logfile;
			while (<LOG>) {
				if (m/^\s*Build action:\s*(.*?)\s*$/) {
					$buildaction = $1;
#				} elsif (m/^CHROOTED=(.*?)\s*$/) {
#					$chrooted = $1;
				} elsif (m/^RC_ARCHS=\s*(.*?)\s*$/) {
					$rcarchs = $1;
				} elsif (m/^EXIT STATUS: ([0-9]+)$/) {
					$exitstatus = $1;
				}
			}
			close LOG;
		}
	}

	if ($exitstatus eq "0") {
		$color = q(bgcolor="#AAFFAA"); # GREEN
	} else {
		$color = q(bgcolor="#FFAAAA"); # RED
	}

	if ($logfile) {
		$logfile = basename($logfile);
		my $logname = $logfile;
		$logname =~ s/.log~/~/;
		print <<EOB;
<tr>
  <td><a href="$projnam/$logfile">$logname</a></td>
  <td><small>$buildaction</small></td>
  <td><small>$rcarchs</small></td>
  <td align="right" $color>$exitstatus</td>
</tr>
EOB
	} else {
		print <<EOB;
<tr>
  <td>$project</td>
  <td></td>
  <td></td>
  <td></td>
EOB
	}
}

print <<EOB;
</tbody>
</table>
</body>
</html>
EOB
