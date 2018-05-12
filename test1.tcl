load ./libch341a[info sharedlibextension] ch341a
ch341a c
puts "capacity: [c capacity]"

set f [open /home/user/coreboot-new2/build/coreboot.rom rb]
set content [read $f]
close $f

puts "Erasing chip..."
c erase
while {[c status] != 0} {
    puts -nonewline "."
    flush stdout
    after 1
}
puts
puts "Blank check..."
c verbose 0
for {set addr 0} {$addr < [string length $content]} {incr addr 512} {
    set x [c read $addr 512]
    if {$x eq [string repeat \xff 512]} {
        puts -nonewline .
    } else {
        puts -nonewline X
    }

    if {($addr/512 % 128) == 0} {puts ""}
}

puts "Writing data..."
c verbose 0
for {set addr 0} {$addr < [string length $content]} {incr addr 512} {
    set x [c read $addr 512]
    set x2 [string range $content $addr $addr+511]
    if {$x2 eq $x} {
        puts -nonewline "."  ;# already the right value
    } else {
        c write $addr $x2
        set x3 [c read $addr 512]
        if {$x3 eq $x2} {
            puts -nonewline ","  ;# successfully written
        } else {
            puts -nonewline "x"  ;# couldn't be written
        }
    }

    if {($addr/512 % 128) == 0} {puts ""}
}
