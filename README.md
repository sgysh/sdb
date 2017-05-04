# sdb
simple debugger for x86_64 Linux

by Yoshinori Sugino

---

## License
MIT

---

## Usage example

    % nm sample | grep func
    000000000040057d T func
    % ./sdb sample
    (sdb) b 0x000000000040057d
    (sdb) r
    Continuing
    stop
    Breakpoint at 0x40057d
    (sdb) s
    stop
    (sdb) p $pc
    0x40057e
    (sdb) c
    Continuing
    count: 0
    stop
    Breakpoint at 0x40057d
    (sdb) q

    b: break, r: run, s: step, p: print, c: continue, q: quit

