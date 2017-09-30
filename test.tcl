#####################################################################
puts "Checking comment handling"

proc test {} {
# skip this
	puts "Hello"
#and this
	puts "Goodbye"
#and this
}

puts "Expect Hello Goodbye"

puts [test]

#####################################################################
puts "\nTesting set"

set v {}
puts "Expect empty string"
set v
set v {A B C}
puts "Expect A B C"
puts [set v]

#####################################################################
puts "\nTesting strings"

puts "Expect p"
puts [string index "Sample string" 3]
puts "Expect ple s"
puts [string range "Sample string" 3 7]
puts "Expect ple string"
puts [string range "Sample string" 3 end]
puts "Expect 9"
puts [string first th "There is the tub where I bathed today"]
puts "Expect 27"
puts [string last th "There is the tub where I bathed today"]
puts "Expect -1"
puts [string compare Michigan Minnesota]
puts "Expect 0"
puts [string compare Michigan Michigan]
puts "Expect 13"
puts [string length "sample string"]
puts "Expect WATCH OUT!"
puts [string toupper "Watch out!"]
puts "Expect 15 charing cross road"
puts [string tolower "15 Charing Cross Road"]
puts "Expect xxx"
puts [string trim aaxxxbab abc]

#####################################################################
puts "\nTesting immediate expressions"

puts "Expect 650"
puts [expr 10 * 65]

puts "Expect Result is 650"
set x 10
set y 65
set res [expr $x*$y]
puts "Result is $res"

#####################################################################
puts "\nTesting arrays"

puts "Expect 218"

set matrix(1,1)	140
set matrix(1,2)	218
set matrix(1,3)	84
set i 1
set j 2
set cell $matrix($i,$j)
puts $cell

puts "Expect environment variable list"

puts [array names env]

puts "Expect value of PATH environment variable"

puts [set env(PATH)]

#####################################################################
puts "\nTesting procedures"


proc msg x { puts $x }

puts "Expect Hello"
msg Hello
puts "Expect Goodbye"
msg Goodbye

proc msg2 {x y} { puts "$x $y" }

puts "Expect Hello Gram"
msg2 Hello Gram
puts "Expect Goodbye Gram"
msg2 Goodbye Gram

proc inc {value {increment 1}} {
    expr $value+$increment
}

puts [inc 42 3]
puts "Expect 45"
puts [inc 42]
puts "Expect 43"

#####################################################################
puts "\nTesting regexp"

puts "Expect 1"
puts [regexp {^[0-9]+$} 510]
puts "Expect 0"
puts [regexp {^[0-9]+$} -510]

#####################################################################
puts "Testing procedure renaming"

puts "Expect ZIP"
rename puts oldputs

proc puts s {
}

puts "Shouldn't see me!"
oldputs ZIP
rename puts xyz
rename oldputs puts

#####################################################################
puts "\nTesting the scan command"

scan "The 3 cats sat" "The %d %s" cnt creature
puts "Expect There were 3 cats"
puts "There were $cnt ${creature}"

proc next c {
	scan $c %c i
	format %c [expr $i+1]
}

puts "Expect b"
puts [next a]
puts "Expect :"
puts [next 9]

#####################################################################
puts "\nChecking info default command"

proc dummy { {a 2} b {c 99} d} {
	puts Wow!
}

puts "Expect 1"
puts [info default dummy a x]
puts "Expect 2"
puts $x
puts "Expect 0"
puts [info default dummy b x]
puts "Expect 1"
puts [info default dummy c x]
puts "Expect 99"
puts $x
puts "Expect 0"
puts [info default dummy d x]

#####################################################################
puts "\nTesting upvar/uplevel"

proc do {varname first last body} {
	upvar $varname v
	for {set v $first} {$v <= $last} {incr v} {
		uplevel $body
	}
}

set v {}
do i 1 5 {
	lappend v [expr $i*$i]
}

puts "EXPECT 1 4 9 16 25"
puts [set v]

rename do xxyyzz

proc do {varname first last body} {
	upvar $varname v
	for {set v $first} {$v <= $last} {incr v} {
		set code [catch {uplevel $body} string]
		if {$code == 1} {
			return 1 $string
		} elseif {$code == 2} {
			return 2 $string
		} elseif {$code == 3} {
			return
		} elseif {$code > 4} {
			return $code $string
		}
	}
}

set v {}
do i 1 5 {
	lappend v [expr $i*$i]
}

puts "EXPECT 1 4 9 16 25"
puts [set v]

####################################################################
puts "\nTesting file handling"

puts "Expect \"the\ncat\nthe cat\"\n"
puts "the"
puts "cat"
puts -nonewline "the "
puts "cat"

set f [open "junk" "w"]
puts $f "puts \"the cat sat on the mat!\""
close $f

puts "Expect the cat sat on the mat!"
puts [source junk]

####################################################################
puts "\nTesting foreach"

puts "Expect 15"

proc sum args {
	set s 0
	foreach i $args {
		incr s $i
	}
	puts $s
}

sum 1 2 3 4 5

####################################################################
puts "\nTesting switch"

set a cat
puts "Expect Hello"
switch $a {
 bat { puts Goodbye }
 rat { puts Wow! }
 cat { puts Hello }
 hat { puts Yow! }
}
set a hat
puts "Expect Yow!"
switch $a bat { puts Goodbye } rat { puts Wow! } cat { puts Hello } hat { puts Yow! }

puts "Expect Wow!"
switch $a bat { puts Goodbye } cat { puts Hello } rat { puts Yow! } default { puts "Wow!" }

set a rat
puts "Expect Yow!"
switch $a {
 bat { puts Goodbye }
 rat -
 cat -
 hat { puts Yow! }
}

####################################################################
puts "\nGeneral syntax tests"

set msg Eggs:\ \$2.18/dozen\nGasoline:\ \$1.19/gallon
puts "Expect  Eggs: \$2.18/dozen"
puts "       Gasoline: \$1.49/gallon"
puts \
$msg

set msg2 "Eggs: \$2.18/dozen\nGasoline: \$1.19/gallon"
puts "Expect  Eggs: \$2.18/dozen"
puts "       Gasoline: \$1.49/gallon"
puts \
$msg2

set msg3 "Eggs: \$2.18/dozen
Gasoline: \$1.19/gallon"
puts "Expect  Eggs: \$2.18/dozen"
puts "       Gasoline: \$1.49/gallon"
puts \
$msg3

set msg3a {Eggs: $2.18/dozen
Gasoline: $1.19/gallon}
puts "Expect Eggs: \$2.18/dozen"
puts "       Gasoline: \$1.49/gallon"
puts \
$msg3a

puts "Expect a is 2; the square of a is 4"
set a 2
set msg4 "a is $a; the square of a is [expr $a*$a]"
puts $msg4

puts "Expect \"Couldn't open file \"a.out\"\""
set name a.out
set msg5 "Couldn't open file \"$name\""
puts $msg5


#This is a comment
puts "Should get an error, but we handle the next bad comment..."

set a 100	# not a comment

puts "Expect 101"

set b 101	;# comment
puts $b

####################################################################
puts "\nTesting format\nExpect hex table from 1 to 16"

foreach i {1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16} {
	puts [format "%2d in decimal is %2x in hex" $i $i]
}

####################################################################
puts "\nTesting glob\nExpect a list of *.cpp and *.h files\n"

puts [glob *.cpp *.h]

####################################################################
puts "\nTesting global"

proc test {} {
	global a
	set a "LocalA"
	set b "LocalB"
	puts "Expect LocalA is LocalA"
	puts "Local A is $a"
	puts "Expect LocalB is LocalB"
	puts "Local B is $b"
}

set a "GlobalA"
set b "GlobalB"

test

puts "Expect GlobalA is LocalA"
puts "Global A is $a"
puts "Expect GlobalB is GlobalB"
puts "Global B is $b"

####################################################################
puts "\nTesting while"

puts "Expect factorial table"

proc fact lim {
	set i $lim
	while {$i>0} {
	    set j $i
	    set f 1
	    while {$j>0} {
	        set f [expr $f*$j]
	        set j [expr $j-1]
	    }
	    puts "Factorial of $i is $f"
	    set i [expr $i-1]
	}
}

fact 6

####################################################################
puts "Testing if"


puts "Expect \"Hello\nGoodbye\""

if {"hat" == "hat"} {
	puts "Hello"
} else {
	puts "Goodbye"
}
if {"cat" == "hat"} {
	puts "Hello"
} else {
	puts "Goodbye"
}

proc test x {
	if {$x == 1} {
		return "one"
	} elseif {$x == 2} {
		return "two"
		return 2 $string
	} elseif {$x == 3} {
		return "three"
		return
	} elseif {$x > 3} {
		return "more"
	}
	return "zero"
}
puts "Expect \"zero\none\ntwo\nthree\nmore\""
puts [test 0]
puts [test 1]
puts [test 2]
puts [test 3]
puts [test 4]

####################################################################
puts "\nTesting lists"

puts "Expect Anne"
puts [lindex {John Anne Mary Jim} 1]
puts "Expect c d e"
puts [lindex {a b {c d e} f} 2]
puts "Expect a b X Y Z {c d} e"
set x {a b {c d} e}
puts [linsert $x 2 X Y Z]
puts "Expect {X Y} Z a b {c d} e"
puts [linsert $x 0 {X Y} Z]

set x {John Anne Mary Jim}
puts "Expect 0"
puts [lsearch $x John]
puts "Expect 1"
puts [lsearch $x Anne]
puts "Expect 2"
puts [lsearch $x Mary]
puts "Expect 3"
puts [lsearch $x Jim]
puts "Expect -1"
puts [lsearch $x Phil]
puts "Expect 1"
puts [lsearch -glob $x A*]

puts "Expect Anne Jim John Mary"
puts [lsort $x]
puts "Expect Mary John Jim Anne"
puts [lsort -decreasing $x]
puts "Expect 1 10 2"
puts [lsort {10 1 2}]
puts "Expect 1 2 10"
puts [lsort -integer {10 1 2}]

set x a/b/c
set y /usr/include/sys/types.h
puts "Expect a b c"
puts [split $x /]
puts "Expect {} usr include sys types.h"
puts [split $y /]
puts "Expect x {} y z"
puts [split xbaybz ab]
puts "Expect a { } b { } c"
puts [split {a b c} {}]

puts "Expect /usr/include/sys/types.h"
puts [join { {} usr include sys types.h} /]
puts "Expect 141"
set x {24 112 5}
puts [expr [join $x +]]

puts "Expect set x {Earning: \$1410.13}"
set initValue {Earnings: $1410.13}
puts [list set x $initValue]
puts "Expect sr x \\{\\ \\\\"
set initValue "{ \\"
puts [list set x $initValue]

####################################################################
puts "\nTesting traces"

proc tracer {name element op} {
    puts "In tracer($name,$element,$op)"
}

set x 0
trace variable x rw tracer

puts "Expect \"In tracer(x,,w)\nIn tracer(x,,r)\""
set x 0
puts "Expect In tracer(x,,r)"
set x

####################################################################
puts "\nTesting unknown proc"

proc unknown {p args} {
    puts "In unknown $p"
}

puts "Expect In Unknown xxx"
xxx

####################################################################
puts "\nOdds 'n sods"

set msg ""
foreach i {1 2 3 4 5} {
	append msg "$i squared is [expr $i*$i]\n"
}
puts "Expect table of squares from 1 to 5"
puts $msg

set a {This list runs forwards}
set b ""
set i [expr [llength $a] -1]
while {$i >= 0} {
	lappend b [lindex $a $i]
	decr i
}

puts "Expect This list runs forwards"
puts $a
puts "Expect forwards runs list This"
puts $b

set a ""
for {set i [expr [llength $b]-1]} {$i>=0} {decr i} {
	lappend a [lindex $b $i]
}

puts "Expect This list runs forwards"
puts $a

set b ""
foreach i $a {
	set b [linsert $b 0 $i]
}

puts "Expect forwards runs list This"
puts $b

set b ""
foreach i $a {
	if {$i == "runs"} break;
	set b [linsert $b 0 $i]
}

puts "Expect list This"
puts $b

set b ""
foreach i $a {
	if {$i == "runs"} continue;
	set b [linsert $b 0 $i]
}

puts "Expect forwards list This"
puts $b

####################################################################
puts "Testing exceptions"

proc a {} {
	global a
	puts $a(b)
}

proc b {} {
	a
}

proc c {} {
	b
}

set a(a) 2
set a(b) 2
puts "Expect 2"
puts $a(a)
unset a

puts "Expect 1 Undefined variable a(b)"
puts "[catch c res] $res"
puts "Expect Undefined variable a(b) and traceback"
c


