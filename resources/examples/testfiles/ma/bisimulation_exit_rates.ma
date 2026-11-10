ma

const double lambda = 2;

module m
    s : [0..2] init 0;

    <> (s=0) -> lambda : (s'=1) + lambda : (s'=2);
    <> (s=1) -> 1 : true;
    <> (s=2) -> 1 : true;
endmodule

label "done" = s>0;
