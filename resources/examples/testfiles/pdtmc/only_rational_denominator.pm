// A parametric DTMC whose transition probabilities have symbolic denominators. The collected well-formedness
// constraints therefore have to constrain the sign of the denominator instead of relying on it being a positive
// constant.
dtmc

const double p;
const double q;

module test

	// local state
	s : [0..2] init 0;

	[] s=0 -> 1 : (s'=1);
	[] s=1 -> (1-p)/(1-p+q) : (s'=1) + q/(1-p+q) : (s'=2);
	[] s=2 -> 1 : (s'=2);

endmodule

label "target" = s=2;
