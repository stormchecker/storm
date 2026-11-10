dtmc

module test
	s : [0 .. 3] init 0;

	[] s=0 -> 0.5 : (s'=1) + 0.5 : (s'=2);
	[] s=1 -> 1 : true;
	[] s=2 -> 1 : (s'=3);
	[] s=3 -> 1 : true;

endmodule

rewards
	s=2 : 1;
endrewards

label "unreachable" = false;
label "condition" = s=2;
label "target" = s=3;
