mkdir -p tmp
rm tmp/*

set -e
STORM=./cmake-build-debug/bin/storm
EXAMPLES=./resources/examples/testfiles

# brp (DTMC)

$STORM --prism $EXAMPLES/dtmc/brp-16-2.pm --exportbuild tmp/brp-double.umb --buildfull
$STORM -umb tmp/brp-double.umb
$STORM -umb tmp/brp-double.umb --exact

$STORM --prism $EXAMPLES/dtmc/brp-16-2.pm --exportbuild tmp/brp-double-none.umb --compression none
$STORM -umb tmp/brp-double-none.umb

$STORM --prism $EXAMPLES/dtmc/brp-16-2.pm --exportbuild tmp/brp-double-xz.umb --compression xz
$STORM -umb tmp/brp-double-xz.umb

$STORM --prism $EXAMPLES/dtmc/brp-16-2.pm --exportbuild tmp/brp-exact.umb --exact
$STORM -umb tmp/brp-exact.umb
$STORM -umb tmp/brp-exact.umb --exact

# polling (MA)

$STORM --prism $EXAMPLES/ma/polling.ma -const N=3,Q=3 --exportbuild tmp/pol-double.umb --buildchoicelab
$STORM -umb tmp/pol-double.umb
$STORM -umb tmp/pol-double.umb --buildchoicelab
$STORM -umb tmp/pol-double.umb --exact

# robot (IMDP)
$STORM --prism $EXAMPLES/imdp/robot.prism -const delta=0.5 --exportbuild tmp/robot-double.umb
$STORM -umb tmp/robot-double.umb
$STORM -umb tmp/robot-double.umb --exact
$STORM --prism $EXAMPLES/imdp/robot.prism -const delta=0.5 --exportbuild tmp/robot-exact.umb --exact
$STORM -umb tmp/robot-exact.umb
$STORM -umb tmp/robot-exact.umb --exact


# robot (IMDP), DRN
$STORM --prism $EXAMPLES/imdp/robot.prism -const delta=0.5 --exportbuild tmp/robot-double.drn
$STORM -drn tmp/robot-double.drn
$STORM -drn tmp/robot-double.drn --exact
$STORM --prism $EXAMPLES/imdp/robot.prism -const delta=0.5 --exportbuild tmp/robot-exact.drn --exact
$STORM -drn tmp/robot-exact.drn
$STORM -drn tmp/robot-exact.drn --exact


# maze (POMDP)
$STORM --prism $EXAMPLES/pomdp/maze2.prism -const sl=0.5 --exportbuild tmp/maze2-double.umb --buildfull --buildchoicelab
$STORM -umb tmp/maze2-double.umb --buildchoicelab

$STORM --prism $EXAMPLES/pomdp/maze2.prism -const sl=0.5 --exportbuild tmp/maze2-rational.umb --exact --buildfull --buildchoicelab
$STORM -umb tmp/maze2-rational.umb --buildchoicelab

