#!/bin/bash

#
# 5-second regression test for dump/verify/restore. It's not intended to be
# an exhaustive regression test, just a quick way to verify that you haven't
# introduced any errors when changing code.
#
# N.B., this only verifies that THIS build of dump(8) and THIS build of
# restore(8) will play well together. It does not guarantee that these systems
# are compatible with released versions of the software! For that we need
# to keep images of known-good partitions and dump files.
#
# Author: Bear Giles (bgiles@coyotesong.com)
# License granted to dump project under non-advertising BSD license.
#

#
# Create 10 MB virtual partition.
#
# mkvirtpart(filename, loop device)
#
mkvirtpart()
{
    FILENAME=$1
    LOOPDEV=$2
    
    if [ "$#" -ne "2" ]; then
      /bin/echo "usage: mkvrtpart FILENAME LOOPDEV"
      return 1
    fi
    
    # create 10M sparse file
    /usr/bin/truncate -s 10M $FILENAME
    if [ "$?" -ne "0" ]; then
      /bin/echo "unable to create partition image."
      return 1
    fi
    
    # mount and format it
    /sbin/losetup $LOOPDEV $FILENAME
    if [ "$?" -ne "0" ]; then
      /bin/echo "setting up loop device failed."
      return 1
    fi
    
    /sbin/mkfs -text4 $LOOPDEV
    if [ "$?" -ne "0" ]; then
      /bin/echo "formating test partition failed."
      /sbin/losetup -d $LOOPDEV
      return 1
    fi
    
    /sbin/losetup -d $LOOPDEV
    if [ "$?" -ne "0" ]; then
      /bin/echo "tearing down loop device failed."
      return 1
    fi
}

#
# Populate test filesystem
#
# mktestfs(root)
mktestfs()
{
    ROOT=$1
    
    if [ "$#" -ne "1" ]; then
      /bin/echo "usage: mktestfs ROOT"
      return 1
    fi

    if [ "$ROOT" == "" -o "$ROOT" == "/" ]; then
        /bin/echo "cowardly refusing to stomp on root."
        return 1
    fi
    
    /usr/bin/install -d $ROOT
    
    # create typical file
    /usr/bin/touch $ROOT/perm644
    /bin/chmod 0644 $ROOT/perm644
    
    # create typical executable
    /usr/bin/touch $ROOT/perm755
    /bin/chmod 0755 $ROOT/perm755
    
    # create multiple symlinks
    /usr/bin/touch $ROOT/symlink
    /bin/ln $ROOT/symlink $ROOT/symlink1
    /bin/ln $ROOT/symlink $ROOT/symlink2
    
    # create hard links
    /usr/bin/touch $ROOT/hardlink
    /bin/ln $ROOT/hardlink $ROOT/hardlink1
    /bin/ln $ROOT/hardlink $ROOT/hardlink2
    
    # create block device
    /bin/mknod $ROOT/block b 10 20
    
    # create character device
    /bin/mknod $ROOT/char c 11 21
    
    # create FIFO
    /bin/mknod $ROOT/pipe p
    
    # make sparse device
    #/usr/bin/truncate -s 500k $ROOT/sparse
    
    # populate some files
    /bin/mkdir $ROOT/man1
    /bin/cp -rp /usr/share/man/man1/* $ROOT/man1
}

#
# Single test cycle
#
dump_verify_restore() {    
    
    if [ "$#" -lt "5" ]; then
      /bin/echo "usage: dump_verify_restore SRC_LOOPDEV SRC_MOUNTPOINT DEST_LOOPDEV DEST_MOUNTPOINT DUMPFILE ..."
      return 1
    fi

    SRC_LOOPDEV=$1
    SRC_MOUNTPOINT=$2
    DEST_LOOPDEV=$3
    DEST_MOUNTPOINT=$4
    DUMPFILE=$5
    
    shift; shift; shift; shift; shift

    /sbin/losetup $SRC_LOOPDEV $SRC_FILENAME
    if [ "$?" -ne "0" ]; then
      /bin/echo "setting up loop device failed."
      return 1
    fi

    # we have to mount partition for verify to work even if we dump the
    # underlying partition.
    /bin/mount $SRC_LOOPDEV $SRC_MOUNTPOINT
    if [ "$?" -ne "0" ]; then
        /bin/echo "mounting source partition failed."
        /sbin/losetup -d $SRC_LOOPDEV
        return 1;
    fi

    # dump the test partition
    ../dump/dump -0 $@ -f $DUMPFILE $SRC_LOOPDEV
    if [ "$?" -ne "0" ]; then
      echo "dump failed, error code $?"
      /bin/rm $DUMPFILE
      /bin/umount $SRC_MOUNTPOINT
      /sbin/losetup -d $SRC_LOOPDEV
      return 1
    fi

    # verify
    ../restore/restore -C -f $DUMPFILE
    if [ "$?" -ne "0" ]; then
      echo "verification failed, error code $?"
      /bin/rm $DUMPFILE
      /bin/umount $SRC_MOUNTPOINT
      /sbin/losetup -d $SRC_LOOPDEV
      return 1
    fi

    # restore fs, compare to orginal one
    # I can't do that yet since restore will only restore to the current directory.
    # this makes sense for a number of reasons it difficult to test our newly
    # compiled code.
    # ../../restore/restore -r ...    
    
    # tear everything down
    /bin/umount $SRC_MOUNTPOINT
    if [ "$?" -ne "0" ]; then
       /bin/echo "unmounting test partition failed."
       return 1    
    fi

    /sbin/losetup -d $SRC_LOOPDEV
    if [ "$?" -ne "0" ]; then
      /bin/echo "tearing down loop device failed."
      return 1
    fi
}

#
# set up source partition.
#
setup_src_partition() {
    SRC_FILENAME=$1
    SRC_LOOPDEV=$2
    SRC_MOUNTPOINT=$3
    
    if [ "$#" -ne "3" ]; then
      /bin/echo "usage: setup_src_partition SRC_FILENAME SRC_LOOPDEV SRC_MOUNTPOINT"
      return 1
    fi
    
    mkvirtpart $SRC_FILENAME $SRC_LOOPDEV
    if [ $? -ne 0 ]; then
       /bin/echo "creating source test partition failed."
       return 1
    fi

    # mount it
    /sbin/losetup $SRC_LOOPDEV $SRC_FILENAME
    if [ "$?" -ne "0" ]; then
       /bin/echo "setting up loop device failed."
       return 1
    fi

    /bin/mount $SRC_LOOPDEV $SRC_MOUNTPOINT
    if [ "$?" -ne "0" ]; then
       /bin/echo "mounting test partition failed."
       return 1
    fi

    mktestfs $SRC_MOUNTPOINT
    if [ "$?" -ne "0" ]; then
       return 1
    fi
      
    /bin/umount $SRC_LOOPDEV
    if [ "$?" -ne "0" ]; then
       /bin/echo "unmounting test partition failed."
       return 1
    fi
      
    /sbin/losetup -d $SRC_LOOPDEV
    if [ "$?" -ne "0" ]; then
      /bin/echo "tearing down loop device failed."
      return 1
    fi
    
    return 0
}


#
# clean up temporary files. We want to be extremely careful here that
# we don't accidently do a 'rm -rf' on / 
#
cleanup() {
    
    if [ "$#" -ne "6" ]; then
      /bin/echo "usage: cleanup SRC_FILENAME SRC_MOUNTPOINT DEST_FILENAME DEST_MOUNTPOINT BASEDIR DUMPFILE"
      return 1
    fi
    
    SRC_FILENAME=$1
    SRC_MOUNTPOINT=$2
    DEST_FILENAME=$3
    DEST_MOUNTPOINT=$4
    BASEDIR=$5
    DUMPFILE=$6

    if [ "$BASEDIR" == "" -o "$BASEDIR" == "/" ]; then
        /bin/echo "cowardly refusing to delete root."
        return 1
    fi

    # we don't do rm -r since we don't want to delete
    # anything we didn't create.
    /bin/rm -f $SRC_FILENAME
    /bin/rmdir $SRC_MOUNTPOINT
    /bin/rm -f $DEST_FILENAME
    /bin/rmdir $DEST_MOUNTPOINT
    /bin/rm -f $DUMPFILE
    /bin/rmdir $BASEDIR
   
    return 0 
}

###############################################
#
# the actual script
#
BASEDIR=`/bin/mktemp -d`

SRC_FILENAME=$BASEDIR/dump-test-src.img
SRC_LOOPDEV=/dev/loop6
SRC_MOUNTPOINT=$BASEDIR/src
DEST_FILENAME=$BASEDIR/dump-test-dst.img
DEST_LOOPDEV=/dev/loop7
DEST_MOUNTPOINT=$BASEDIR/dest
DUMPFILE=$BASEDIR/dump-test.dump

/bin/echo BASEDIR = $BASEDIR

/usr/bin/install -d $BASEDIR
/usr/bin/install -d $SRC_MOUNTPOINT
/usr/bin/install -d $DEST_MOUNTPOINT

# Setup source partition
setup_src_partition $SRC_FILENAME $SRC_LOOPDEV $SRC_MOUNTPOINT
if [ $? -ne 0 ]; then
   /bin/echo "creating source test partition failed."
   cleanup $SRC_FILENAME $SRC_MOUNTPOINT $DEST_FILENAME $DEST_MOUNTPOINT $BASEDIR $DUMPFILE
   exit 1
fi

# create dest partition (for restores)
mkvirtpart $DEST_FILENAME $DEST_LOOPDEV
if [ $? -ne 0 ]; then
   /bin/echo "creating destination test partition failed."
   cleanup $SRC_FILENAME $SRC_MOUNTPOINT $DEST_FILENAME $DEST_MOUNTPOINT $BASEDIR $DUMPFILE
   exit 1
fi

echo
echo "testing basic dump/restore" 
dump_verify_restore $SRC_LOOPDEV $SRC_MOUNTPOINT $DEST_LOOPDEV $DEST_MOUNTPOINT $DUMPFILE
if [ $? -ne 0 ]; then
   /bin/echo "dump cycle failed."
   cleanup $SRC_FILENAME $SRC_MOUNTPOINT $DEST_FILENAME $DEST_MOUNTPOINT $BASEDIR $DUMPFILE
   exit 1
fi

echo
echo "testing compressed dump/restore (lzo)..."
dump_verify_restore $SRC_LOOPDEV $SRC_MOUNTPOINT $DEST_LOOPDEV $DEST_MOUNTPOINT $DUMPFILE -y
if [ $? -ne 0 ]; then
   /bin/echo "dump cycle failed."
   cleanup $SRC_FILENAME $SRC_MOUNTPOINT $DEST_FILENAME $DEST_MOUNTPOINT $BASEDIR $DUMPFILE
   exit 1
fi

echo
echo "testing compressed dump/restore (zlib)..."
dump_verify_restore $SRC_LOOPDEV $SRC_MOUNTPOINT $DEST_LOOPDEV $DEST_MOUNTPOINT $DUMPFILE -z2
if [ $? -ne 0 ]; then
   /bin/echo "dump cycle failed."
   cleanup $SRC_FILENAME $SRC_MOUNTPOINT $DEST_FILENAME $DEST_MOUNTPOINT $BASEDIR $DUMPFILE
   exit 1
fi

echo
echo "testing compressed dump/restore (bzlib)..."
dump_verify_restore $SRC_LOOPDEV $SRC_MOUNTPOINT $DEST_LOOPDEV $DEST_MOUNTPOINT $DUMPFILE -j2
if [ $? -ne 0 ]; then
   /bin/echo "dump cycle failed."
   cleanup $SRC_FILENAME $SRC_MOUNTPOINT $DEST_FILENAME $DEST_MOUNTPOINT $BASEDIR $DUMPFILE
   exit 1
fi

cleanup $SRC_FILENAME $SRC_MOUNTPOINT $DEST_FILENAME $DEST_MOUNTPOINT $BASEDIR $DUMPFILE

/bin/echo "#"
/bin/echo "# success!"
/bin/echo "#"

exit 0
