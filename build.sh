#!/bin/bash
# Fast build script for FUSE
# login to thoth:
#     ssh PITT_ID@thoth.cs.pitt.edu
# Navigate to your working directory:
#      cd /u/OSLab/PITT_ID/WORK_DIR
# Give execute permission to the script:
#      chmod +x build.sh 
# Run the script:
#      ./build.sh
# (c) Mohammad H. Mofrad, 2016
# (e) hasanzadeh@cs.pitt.edu

BASE=$PWD;                    # Current working directory 
FUSE_SRC="fuse-2.7.0.tar.gz"; # FUSE source file
FUSE_DIR="fuse-2.7.0";        # FUSE source directory
TEST_MOUNT="testmount";
EXAMPLE=$1

# Check if you are at /u/OSLab/PITT_ID
# If yes, continue
# If not,  exit
# Comment this part if the regular expression is not 
# compatible with your PITT_ID but you are working under
# /u/OSLab/PITT_ID directory
if [[ "$BASE" =~ ^/u/OSLab/[a-zA-Z]{3}[0-9]{1,3}/.*$ ]];
then
     echo "$BASE OK.";
     if [[ "$BASE" =~ (/u/OSLab/)([a-zA-Z]{3}[0-9]{2,4}) ]];
     then 
          PITT_ID=${BASH_REMATCH[2]};
          echo "Your PITT_ID $PITT_ID";
     fi
else
     echo "Must change $BASE to /u/OSLab/";
     exit;
fi;

# Check if you have the FUSE directory in your working directory
# If not, extract fuse directory
# If yes, do nothing
if [ -f "$FUSE_SRC" ];
then
     echo "$FUSE_SRC found.";
     echo "You can always remove the current build using \"rm -rf $FUSE_DIR*\"";
else
     echo "$FUSE_SRC not found.";
     cp /u/OSLab/original/$FUSE_SRC .;
     echo "Extracting $FUSE_SRC into $PWD";
     tar xzvf $FUSE_SRC;
     cd $PWD/$FUSE_DIR;
     echo "Installing $FUSE_DIR into $PWD";
     ./configure;
     make;
     cd -;
     echo "Build compeleted!";
fi

if [ $# -eq 0 ];
  then
    echo "To run FUSE examples use \"$0 example\"";
    echo "USAGE: \"$0 <hello|cs1550>\"";
    exit;
fi

if [ $EXAMPLE = "hello" ];
then
   echo "Running $1 example";
   cd $PWD/$FUSE_DIR/example/;
   if [ ! -d "$TEST_MOUNT" ]; 
   then
       echo "Creating $TEST_MOUNT";
       mkdir $TEST_MOUNT
   fi
   echo
   echo "Listing $TEST_MOUNT directory before running $EXAMPLE example";
   ls -la $TEST_MOUNT
   ./$1 $TEST_MOUNT 
   echo
   echo "Listing $TEST_MOUNT directory after  running $EXAMPLE example";
   ls -la $TEST_MOUNT
   echo
   echo "Concatenating $TEST_MOUNT/$EXAMPLE file";
   cat $TEST_MOUNT/$EXAMPLE
   echo
   echo "Unmounting $TEST_MOUNT";
   fusermount -u $TEST_MOUNT
elif [ $EXAMPLE = "cs1550" ];
then
   echo "Running $1 example";
   cd $PWD/$FUSE_DIR/example/;
   if [ ! -d "$TEST_MOUNT" ]; 
   then
       echo "Creating $TEST_MOUNT";
       mkdir $TEST_MOUNT
   fi
   if [ ! -f ".disk" ]; then
       echo
       echo "Creating disk image at \".disk\"";
       dd bs=1K count=5K if=/dev/zero of=.disk
   fi   
   echo
   echo "Unmounting the previous $TEST_MOUNT";
   fusermount -u $TEST_MOUNT
   echo
   echo "Cleaning the previous build";
   make clean
   echo
   echo "Making the new build";
   make
   echo
   echo "Launching FUSE daemon for \"$EXAMPLE\"";
   ./$EXAMPLE -d $TEST_MOUNT
fi