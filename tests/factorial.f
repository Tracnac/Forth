( Large Factorial )

: +! ( n adr -- )
 DUP @ ROT + SWAP ! ;

: U.R ( u n -- )
 SWAP 0 <# #S #>
 ROT OVER -
 SPACES TYPE ;

DEFINER BYTE-ARRAY ( n -- )
 ALLOT
DOES>m + ;                ( n -- adr )

4000 CONSTANT MAX-DIGITS
MAX-DIGITS BYTE-ARRAY F-BUFF
0 VARIABLE LAST ( Last buff element )

: *BUFF ( Multiplier )
 0                  ( Carry )
 LAST @ 1+ 0
 DO
  OVER I F-BUFF C@
  * + 10 /MOD
  SWAP I F-BUFF C!
 LOOP
 BEGIN ( Extend buffer to accept final carry )
  ?DUP
 WHILE
  10 /MOD SWAP
  1 LAST +!
  LAST @ DUP 1+
  MAX-DIGITS >3
   IF
    ." Out of buffer" QUIT
   THEN
  F-BUFF C!
 REPEAT
 DROP ;

: SETUP
 1 0 F-BUFF C! ( Start buff=1 )
 0 LAST ! ;

: .FAC
 LAST @ 1+ 0
 DO
  LAST @ I -
  DUP 1+ 3 MOD
  0= I 0= 0= AND
  IF
   ASCII , EMIT
  THEN
  F-BUFF C@ 1 U.R
 LOOP ;

: FAC
 SETUP 1+ 1
 DO
  I *BUFF
 LOOP ;

: FACS
 SETUP 1+ 1
 DO
  I *BUFF ." Factorial"
  I 3 U.R
  ."  = " .FAC CR
 LOOP ;
 