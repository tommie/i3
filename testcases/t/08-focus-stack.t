#!perl
# vim:ts=4:sw=4:expandtab
# Checks if the focus is correctly restored, when creating a floating client
# over an unfocused tiling client and destroying the floating one again.

use Test::More tests => 4;
use Test::Deep;
use X11::XCB qw(:all);
use Data::Dumper;
use Time::HiRes qw(sleep);
use FindBin;
use lib "$FindBin::Bin/lib";
use i3test;
use AnyEvent::I3;

BEGIN {
    use_ok('X11::XCB::Window') or BAIL_OUT('Could not load X11::XCB::Window');
}

my $x = X11::XCB::Connection->new;

my $i3 = i3;

# Switch to the nineth workspace
$i3->command('9')->recv;

my $tiled_left = i3test::open_standard_window($x);
my $tiled_right = i3test::open_standard_window($x);

sleep(0.25);

$i3->command('ml')->recv;

# Get input focus before creating the floating window
my $focus = $x->input_focus;

# Create a floating window which is smaller than the minimum enforced size of i3
my $window = $x->root->create_child(
    class => WINDOW_CLASS_INPUT_OUTPUT,
    rect => [ 1, 1, 30, 30],
    background_color => '#C0C0C0',
    type => $x->atom(name => '_NET_WM_WINDOW_TYPE_UTILITY'),
);

isa_ok($window, 'X11::XCB::Window');

$window->map;

sleep(0.25);
is($x->input_focus, $window->id, 'floating window focused');

$window->unmap;

sleep(0.25);

is($x->input_focus, $focus, 'Focus correctly restored');

diag( "Testing i3, Perl $], $^X" );
