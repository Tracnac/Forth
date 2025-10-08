\ Simple large factorial test

4000 CONSTANT MAX-DIGITS

VARIABLE F-BUFF-START
HERE F-BUFF-START !
100 ALLOT  \ Start with smaller buffer for testing

: F-BUFF ( n -- addr )
  F-BUFF-START @ + ;

VARIABLE LAST

: SETUP
  1 0 F-BUFF C!
  0 LAST ! ;

: *BUFF ( multiplier -- )
  0                       \ Carry
  LAST @ 1+ 0 DO
    OVER I F-BUFF C@
    * +
    10 /MOD
    SWAP I F-BUFF C!
  LOOP
  
  \ Handle remaining carry
  BEGIN
    ?DUP
  WHILE
    10 /MOD SWAP
    1 LAST +!
    LAST @ F-BUFF C!
  REPEAT
  DROP ;

: .FAC
  LAST @ 1+ 0 DO
    LAST @ I - F-BUFF C@
    48 + EMIT
  LOOP ;

: FAC ( n -- )
  SETUP
  1+ 1 DO
    I *BUFF
  LOOP ;

\ Tests
." 10! = "
10 FAC .FAC CR

." 20! = "
20 FAC .FAC CR

." 50! = "
50 FAC .FAC CR
.
BYE
