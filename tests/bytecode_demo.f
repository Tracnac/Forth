\ Bytecode Save/Load Demo
\ This demonstrates the binary bytecode feature

: GREET  ." Hello from bytecode!" CR ;
: SQUARE  DUP * ;
: CUBE  DUP DUP * * ;
: FIBONACCI  ( n -- fib )
  DUP 2 < IF 
    DROP 1
  ELSE
    DUP 1 - FIBONACCI 
    SWAP 2 - FIBONACCI 
    +
  THEN 
;

\ Test words
." Math functions loaded!" CR
5 SQUARE . CR
3 CUBE . CR
