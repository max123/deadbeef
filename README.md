deadbeef
========

This contains a pseudo device driver, kbad, and a test application, kbadtest.

To build and install the device driver you'll need a working C compiler. I
recommend doing this in a SmartOS zone (it's easier to pkgin gcc there).

To build the driver:

   $ gcc -m64 -D_KERNEL -mno-red-zone -c kbad.c
   $ ld -r -o kbad kbad.o

To install, from the global zone:

   # cp kbad /kernel/drv/amd64/
   # cp kbad.conf /kernel/drv/
   # add_drv kbad
   #

(If you get anything other than a prompt back from add_drv, it did not work,
tail /var/adm/messages to try to find the problem).

To build the test program:

   $ gcc -m 64 -o kbadtest kbadtest.o
   $

To run the test (from the global zone):

   # while true; do ./kbadtest; sleep 1; done

If kmem_flags is 0, you should do some stuff in another window.  If kmem_flags
is 0xf, the system should panic in about 30 seconds.  Regardless, this should eventually cause the system to panic.  Don't do this on your production system!
