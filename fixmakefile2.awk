/^XLIBS*/ || /^XREQS*/ {
    ORS="";
    for (n=1; n<=NF; n++) { 
	if ($n ~ /-ldl/) {
	    print "-lcompat "
		} else {
	    print $n " "
		}
    }
    ORS="\n"; print "";
}

! /^XLIBS*/ && ! /^XREQS*/ { 
print $0 
}
