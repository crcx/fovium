variable addr variable shift
: this addr @ ;
: new-word here addr ! 0 shift ! 0 , ;
: shift@+ shift @ dup 6 + shift ! ;
: (op,) ( op -- ) shift@+ lshift this @ or this ! ;
: full? dup -4 and  shift @ 30 = and  shift @ 36 = or ;
: op, full? if new-word then (op,) ;

\ Mnemonics

: ,nexti 0 op, ;
: ,dup 1 op, ;
: ,call 6 lshift 2 + , new-word ;
: ,lit 3 op, , ;
: ,drop 4 op, ;
: ,swap 5 op, ;
: ,over 6 op, ;
: ,nip 7 op, ;

: ,rot 8 op, ;
: ,>r 9 op, ;
: ,>>r 10 op, ;
: ,r@ 11 op, ;
: ,r> 12 op, ;
: ,rdrop 13 op, ;
: ,; 14 op, ;
: ,b 15 op, , ;

: ,?b 16 op, , ;
: ,0b 17 op, , ;
: ,?; 18 op, ;
: ,0; 19 op, ;
: ,t; 20 op, ;
: ,f; 21 op, ;
: ,? 22 op, ;
: ,0= 23 op, ;

: ,= 24 op, ;
: ,< 25 op, ;
: ,& 26 op, ;
: ,| 27 op, ;
: ,^ 28 op, ;
: ,~ 29 op, ;
: ,and 30 op, ;
: ,or 31 op, ;

: ,xor 32 op, ;
: ,not 33 op, ;
: ,>> 34 op, ;
: ,s>> 35 op, ;
: ,<< 36 op, ;
: ,<<> 37 op, ;
: ,+ 38 op, ;
: ,- 39 op, ;

: ,* 40 op, ;
: ,/ 41 op, ;
: ,/mod 42 op, ;
: ,1+ 43 op, ;
: ,1- 44 op, ;
: ,4+ 45 op, ;
: ,4- 46 op, ;
: ,4* 47 op, ;

: ,*+ 48 op, ;
: ,>a 49 op, ;
: ,a 50 op, ;
: ,@a 51 op, ;
: ,!a 52 op, ;
: ,+@ 53 op, ;
: ,b+@ 54 op, ;
: ,+! 55 op, ;

: ,b+! 56 op, ;
: ,@ 57 op, ;
: ,! 58 op, ;
: ,h@ 59 op, ;
: ,h! 60 op, ;
: ,b@ 61 op, ;
: ,b! 62 op, ;
: ,sc 63 op, ;


\ Syscalls

: ,syscall ,lit ,sc ;

: ,exit 0 ,syscall ;
: ,bye 0 ,exit ;

: ,emit 16 ,sc ;

\ Writing an Image File

variable ifile variable ibase
: begin-image here ibase ! new-word ;
: write-image ( c-addr u ) w/o bin create-file
	if abort" Could not open image file" then
	ifile !
	ibase @ here over - ifile @ write-file
	ifile @ close-file ;
