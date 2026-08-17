dtmc

const double p0;

module nonsimple
    s : [0..3] init 0;
    
    [] s=0 ->
        p0 : (s'=0)
        1-p0 : (s'=1);
    [] s=1 -> 1/1 : (s'=2) + (1-1/1) : (s'=3);
    
endmodule

label "target" = s=2;

