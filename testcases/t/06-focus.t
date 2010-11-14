#!perl
# vim:ts=4:sw=4:expandtab
# Beware that this test uses workspace 9 to perform some tests (it expects
# the workspace to be empty).
# TODO: skip it by default?

use Test::More tests => 13;
use Test::Deep;
use X11::XCB qw(:all);
use Data::Dumper;
use Time::HiRes qw(sleep);
use FindBin;
use lib "$FindBin::Bin/lib";
use i3test;
use AnyEvent::I3;

BEGIN {
    use_ok('X11::XCB::Connection') or BAIL_OUT('Cannot load X11::XCB::Connection');
}

my $x = X11::XCB::Connection->new;

my $i3 = i3;

# Switch to the nineth workspace
$i3->command('9')->recv;

#####################################################################
# Create two windows and make sure focus switching works
#####################################################################

# Change mode of the container to "default" for following tests
$i3->command('d')->recv;

my $top = i3test::open_standard_window($x);
my $mid = i3test::open_standard_window($x);
my $bottom = i3test::open_standard_window($x);
sleep(0.25);

diag("top id = " . $top->id);
diag("mid id = " . $mid->id);
diag("bottom id = " . $bottom->id);

#
# Returns the input focus after sending the given command to i3 via IPC
# end sleeping for half a second to make sure i3 reacted
#
sub focus_after {
    my $msg = shift;

    $i3->command($msg)->recv;
    return $x->input_focus;
}

$focus = $x->input_focus;
is($focus, $bottom->id, "Latest window focused");

$focus = focus_after("k");
is($focus, $mid->id, "Middle window focused");

$focus = focus_after("k");
is($focus, $top->id, "Top window focused");

#####################################################################
# Test focus wrapping
#####################################################################

$focus = focus_after("k");
is($focus, $bottom->id, "Bottom window focused (wrapping to the top works)");

$focus = focus_after("j");
is($focus, $top->id, "Top window focused (wrapping to the bottom works)");

###############################################
# Test focus with empty containers and colspan
###############################################

# Switch to the 10. workspace
$i3->command('10')->recv;

$top = i3test::open_standard_window($x);
$bottom = i3test::open_standard_window($x);
sleep 0.25;

$focus = focus_after("mj");
$focus = focus_after("mh");
$focus = focus_after("k");
is($focus, $bottom->id, "Selecting top window without snapping doesn't work");

$focus = focus_after("sl");
is($focus, $bottom->id, "Bottom window focused");

$focus = focus_after("k");
is($focus, $top->id, "Top window focused");

# Same thing, but left/right instead of top/bottom

# Switch to the 11. workspace
$i3->command('11')->recv;

my $left = i3test::open_standard_window($x);
my $right = i3test::open_standard_window($x);
sleep 0.25;

$focus = focus_after("ml");
$focus = focus_after("h");
$focus = focus_after("mk");
$focus = focus_after("l");
is($focus, $left->id, "Selecting right window without snapping doesn't work");

$focus = focus_after("sj");
is($focus, $left->id, "left window focused");

$focus = focus_after("l");
is($focus, $right->id, "right window focused");


diag( "Testing i3, Perl $], $^X" );
