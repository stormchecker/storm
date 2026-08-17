// Knuth's model of a fair die using unfair coins, where the bias is controlled by nature.
dtmc

module die

	// local state
	s : [0..7] init 0;
	// value of the dice
	d : [0..6] init 0;
	
	[] s=0 -> [1/3, 2/3] : (s'=1) + [1/3, 2/3] : (s'=2);
	[] s=1 -> [1/3, 2/3] : (s'=3) + [1/3, 2/3] : (s'=4);
	[] s=2 -> [1/3, 2/3] : (s'=5) + [1/3, 2/3] : (s'=6);
	[] s=3 -> [1/3, 2/3] : (s'=1) + [1/3, 2/3] : (s'=7) & (d'=1);
	[] s=4 -> [1/3, 2/3] : (s'=7) & (d'=2) + [1/3, 2/3] : (s'=7) & (d'=3);
	[] s=5 -> [1/3, 2/3] : (s'=7) & (d'=4) + [1/3, 2/3] : (s'=7) & (d'=5);
	[] s=6 -> [1/3, 2/3] : (s'=2) + [1/3, 2/3] : (s'=7) & (d'=6);
	[] s=7 -> 1: (s'=7);
	
endmodule

rewards "coin_flips"
	[] s<7 : 1;
endrewards

label "one" = s=7&d=1;
label "two" = s=7&d=2;
label "three" = s=7&d=3;
label "done" = s=7;
