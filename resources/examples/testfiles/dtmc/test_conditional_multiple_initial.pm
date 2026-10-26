dtmc

module test
	s : [0 .. 3];

	[] s=0 -> 0.5 : (s'=1) + 0.5 : (s'=2);
	[] s=1 -> 1 : true;
	[] s=2 -> 1 : (s'=3);
	[] s=3 -> 1 : true;

endmodule

// The condition is reachable from state 0, but not from state 1.
init
	s=0 | s=1
endinit

rewards
	s=2 : 1;
endrewards

label "start" = s=0;
label "condition" = s=2;
label "target" = s=3;
