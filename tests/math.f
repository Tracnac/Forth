: POWER  ( base exponent -- result ) 1 SWAP 0 DO OVER * LOOP SWAP DROP ;

\ Handy math utilities
: SQR   ( n -- n^2 ) DUP * ;
: CUBE  ( n -- n^3 ) DUP DUP * * ;

\ Factorial (recursive): n -- n!
: FACT  ( n -- n! ) DUP 1 > IF DUP 1 - FACT * ELSE DROP 1 THEN ;

\ Sum 1..n using formula: n -- sum
: SUM   ( n -- sum ) DUP 1 + * 2 / ;

\ Greatest common divisor (Euclid) - recursive
: GCD   ( a b -- gcd ) DUP 0= IF DROP ELSE SWAP OVER MOD GCD THEN ;

\ Least common multiple: a b -- lcm
: LCM   ( a b -- lcm ) 2DUP GCD / * ;

\ Fibonacci
: FIBONACCI  ( n -- fib ) DUP 2 < IF DROP 1 ELSE DUP 1 - FIBONACCI SWAP 2 - FIBONACCI + THEN ;
