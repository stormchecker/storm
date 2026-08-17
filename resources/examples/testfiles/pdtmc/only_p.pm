dtmc

const double p;

module test

	// local state
	s : [0..1] init 0;
	
	[] s=0 -> p : (s'=0) + (1-p) : (s'=1);
	[] s=1 -> 1 : (s'=1);
		
endmodule

label "target" = s=1;
