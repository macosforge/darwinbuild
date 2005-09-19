#!/usr/bin/perl

use strict;

sub CommandAsList {
    my $command = $_[0];
    my @output = split(' ', `$command`);
    return @output;
}

if ($#ARGV < 0 || $#ARGV > 1) {
    print STDERR "Usage: $0 projects.txt [output.txt]\n";
    exit(1);
}

my $projectlist = $ARGV[0];
my $outputlist = $#ARGV == 1 ? $ARGV[1] : "";
my @NoBuild = CommandAsList("darwinxref group nobuild");
my %NoBuildHash = map { $_ => 1 } @NoBuild;

my @Compilers = CommandAsList("darwinxref group compilertools");
my %CompilersHash = map { $_ => 1 } @Compilers;

my @UnbuiltProjects = grep { !defined($NoBuildHash{$_}) } CommandAsList("cat $projectlist");
my %UnbuiltProjectsHash = map { $_ => 1 } @UnbuiltProjects;


# sequenced list of projects
my @BuiltProjectsOrder = ();
my %BuiltProjectsHash = (); # has project built?

my %Dependencies = ();
my %InvertedDependencies = ();

my %DepExceptions = ( "IOKitUser" => { "configd" => 1 },
		      "configd" => { "configd_plugins" => 1});

print "Considering projects: @UnbuiltProjects\n";
print "Compilers: @Compilers\n";

print "Generating dependency graph...";

foreach my $proj (@UnbuiltProjects) {
    my @deps = ();
    my @xrefdeps = CommandAsList("darwinxref dependencies -lib $proj; " .
				 "darwinxref dependencies -staticlib $proj");
#    print "$proj depends on @xrefdeps\n";
    foreach my $xdep (@xrefdeps) {
	# don't depend on ourself, compilers, or projects
	# not being built currently
	if ($xdep eq $proj
	    || $CompilersHash{$xdep}
	    || !defined($UnbuiltProjectsHash{$xdep})
	    || ( defined($DepExceptions{$proj}) && $DepExceptions{$proj}->{$xdep})) {
	    next;
	}
	push @deps, ($xdep);
	if(defined($InvertedDependencies{$xdep})) {
	    push @{$InvertedDependencies{$xdep}}, ( $proj );
	} else {
	    $InvertedDependencies{$xdep} = [ $proj ];
	}
    }
    $Dependencies{$proj} = \@deps;

    # try to make sure there's even an empty list of
    # inverted deps (things that depend on me)
    if(!defined($InvertedDependencies{$proj})) {
	$InvertedDependencies{$proj} = [];
    }
}

print " done\n";

#use Data::Dumper;
#print Dumper(\%Dependencies) . "\n";
#print Dumper(\%InvertedDependencies) . "\n";

print "Sequencing based on dependencies...";

# treat UnbuiltProjects like a circular queue
unshift @UnbuiltProjects, "SENTINEL";
my $builtone = 1;
my $loopdebug = 0;
while ($#UnbuiltProjects > 0) {
    my $proj = shift(@UnbuiltProjects);

    # got back to he beginning. Make sure we're making forward progress
    if($proj eq "SENTINEL") {
	print "SENTINEL reached\n" if $loopdebug;
	if($builtone) {
	    # great!
	    $builtone = 0;
	    goto Unmet;
	} else {
	    print STDERR "Aborting, unmet dependency loop\n";
	    print STDERR "Remaining projects: @UnbuiltProjects\n";
	    exit(1);
	}
    }

    # See if all dependencies have been built
    my @deps = @{$Dependencies{$proj}};
    print "$proj: " if $loopdebug;
    foreach my $dep (@deps) {
	if(!defined($BuiltProjectsHash{$dep})) {
	    # dep hasn't built yet
	    print "$dep unbuilt\n" if $loopdebug;
	    goto Unmet;
	}
    }

    print "all deps built\n" if $loopdebug;

    push @BuiltProjectsOrder, ($proj);
    $BuiltProjectsHash{$proj} = 1;
    $builtone = 1;
    next;

  Unmet:
    push @UnbuiltProjects, ($proj);
}

print " done\n";

print "Build Order: @BuiltProjectsOrder\n";

if ($outputlist ne "") {
    open(OUTPUT, ">$outputlist");
    print OUTPUT join("\n", @BuiltProjectsOrder) . "\n";
    close(OUTPUT);
}
