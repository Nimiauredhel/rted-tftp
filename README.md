# stftpu
### a simple Trivial File Transfer Protocol unit

This is a Linux-based TFTP client & server app with some extra features.
Namely, the client can request file deletion and the *BLKSIZE* field is supported for requesting a range of transfer block sizes.

The server side also supports concurrent client-requested operations via multi-threading,
which I arbitrarily capped to 5 at a time because no one will ever actually use this.

It is operated via a command line interface and will spit out the correct "usage" if you get it wrong,
but a "dialog" based TUI menu is also available via provided bash scripts.

The *make* command will compile and deploy *stftpu* to a 'build' folder,
where the *stftpu* executable may be ran directly with the CLI.
Alternately, you may run *bash start.sh* instead for the 'dialog' based menu interface.
The Makefile also provides shortcuts for these two options: *make run* and *make run-tui* respectively.

Security features: none.
