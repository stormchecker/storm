ma

const int K=2;
const double R=7;

module erlang
	s : [0..6];
	// s=5 sink
	// s=6 done
	chainStage : [0..K-1];

	[goToChain] s=0 -> 1 : (s'=1);
	<> s=1 -> 1 : (s'=2);
	<> s=2 & chainStage<K-1 -> R: (chainStage' = chainStage+1);
	<> s=2 & chainStage=K-1 -> R: (s' = 6);
	[goToProbs] s=0 -> 1 : (s'=3);
	<> s=3 -> 1 : (s'=4);
	<> s=4 -> 0.5 : (s'=6) + 0.5: (s'=5);
	<> s=5 -> true;
	<> s=6 -> true;
endmodule

label "done" = (s=6);