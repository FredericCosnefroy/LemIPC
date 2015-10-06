kill -9 $(pgrep a.out)
kill -9 $(pgrep display)
ipcrm -M 666 & ipcrm -S 666 & ipcrm -Q 666
