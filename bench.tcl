proc test101 x {
	if {$x == 1} { return "one" 
	} elseif {$x == 2} { return "two"
	} elseif {$x == 3} { return "three" 
	} elseif {$x > 3} { return "more"
	}
	return "zero"
}

proc test1 {} {
    test101 0;test101 1;test101 2;test101 3;test101 4
}

proc test2 {} {
	lindex {John Anne Mary Jim} 1
	lindex {a b {c d e} f} 2
	set x {a b {c d} e}
	linsert $x 2 X Y Z
	linsert $x 0 {X Y} Z
}

proc inc {value {increment 1}} {
    expr $value+$increment
}

proc test3 {} {
	inc 42 3
	inc 42
}

proc do {varname first last body} {
	upvar $varname k
	for {set k $first} {$k <= $last} {incr k} {
		uplevel $body
	}
}

proc test4 {} {
	set v {}
	do i 1 5 {
		lappend v [expr $i*$i]
	}
}

proc test5 {} {
	set matrix(1,1)	140
	set matrix(1,2)	218
	set matrix(1,3)	84
	set i 1
	set j 2
	set cell $matrix($i,$j)
}

proc sum args {
	set s 0
	foreach i $args {
		incr s $i
	}
}

proc test6 {} {
	sum 1 2 3 4 5
}

		
proc occur {value list} {
	set count 0
	foreach el $list {
		if {$el==$value} {
			incr count
		}
	}
	return $count
}

proc test7 {} {
	occur Gram {Why Gram Gram Oh Gram ??}
}

proc power num {
	set pow 1
	while {$pow < $num} {
		set pow [expr $pow*2]
	}
}

proc test8 {} {
	power 99
}

proc fac x {
	if {$x <= 1} {
		return 1
	}
	expr $x * [fac [expr $x-1]]
}

proc test9 {} {
	fac 4
}

proc test10 {} {
	set msg ""
	foreach i {1 2 3 4 5} {
		append msg "$i squared is [expr $i*$i]\n"
	}
}

proc test11 {} {
	set a {This list runs forwards}
	set b ""
	set i [expr [llength $a] -1]
	while {$i >= 0} {
		lappend b [lindex $a $i]
		incr i -1
	}
	set a ""
	for {set i [expr [llength $b]-1]} {$i>=0} {incr i -1} {
		lappend a [lindex $b $i]
	}
	set b ""
	foreach i $a {
		set b [linsert $b 0 $i]
	}
	set b ""
	foreach i $a {
		if {$i == "runs"} break;
		set b [linsert $b 0 $i]
	}
	set b ""
	foreach i $a {
		if {$i == "runs"} continue;
		set b [linsert $b 0 $i]
	}
	set b
	set msg ""
	foreach i {1 2 3 4 5} {
		append msg "$i squared is [expr $i*$i]\n"
	}
	set a {This list runs forwards}
	set b ""
	set i [expr [llength $a] -1]
	while {$i >= 0} {
		lappend b [lindex $a $i]
		incr i -1
	}
	set a ""
	for {set i [expr [llength $b]-1]} {$i>=0} {incr i -1} {
		lappend a [lindex $b $i]
	}
	set b ""
	foreach i $a {
		set b [linsert $b 0 $i]
	}
	set b ""
	foreach i $a {
		if {$i == "runs"} break;
		set b [linsert $b 0 $i]
	}
	set b ""
	foreach i $a {
		if {$i == "runs"} continue;
		set b [linsert $b 0 $i]
	}
}

proc fact lim {
	set i $lim
	while {$i>0} {
	    set j $i
	    set f 1
	    while {$j>0} {
	        set f [expr $f*$j]
	        set j [expr $j-1]
	    }
	    set i [expr $i-1]
	}
}

proc test12 {} {
	fact 6
}

proc test {cnt routine} {
	while {$cnt > 0} {
		eval $routine
		set cnt [expr $cnt-1]
	}
}

proc runtests cnt {
	test $cnt test1
	test $cnt test2
	test $cnt test3
	test $cnt test4
	test $cnt test5
	test $cnt test6
	test $cnt test7
	test $cnt test8
	test $cnt test9
	test $cnt test10
	test $cnt test11
	test $cnt test12
}

#set res [time {runtests 5} 10]
set res [time {runtests 5} 1]

puts $res


