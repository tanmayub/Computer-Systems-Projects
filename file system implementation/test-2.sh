#!/bin/bash

# Bash script to test question 2 of homework 1
# Usage :- chmod +x test-2.sh
#          ./test-2.sh

# run make and make clean and also create a new disk image
rm -rf disk.img
./mktest disk.img

#-------------------------------------------------------------------
# TEST CASE# 1 : To check read and write works as expected includes
#                -ve cases for read
#-------------------------------------------------------------------
./homework -cmdline -image disk.img <<EOF > /tmp/test1-output
cd dir1
get file.270
cd ..
put file.270
ls
show /dir1
show /dir5/dir6
quit
EOF

cmp /tmp/test1-output <<EOF || { echo TEST q2.1 FAILED; exit; }
read/write block size: 1000
cmd> cd dir1
cmd> get file.270
cmd> cd ..
cmd> put file.270
cmd> ls
dir1
file.270
file.7
file.A
cmd> show /dir1
error: Is a directory
cmd> show /dir5/dir6
error: No such file or directory
cmd> quit
EOF

if [ $? -eq 0 ]; then
	echo "Test q2.1 passed"
else
	echo "Test q2.1 failed"
fi

#-------------------------------------------------------------------
# TEST CASE# 2 : To check out of disk space during multiple writes
#-------------------------------------------------------------------
./homework -cmdline -image disk.img <<EOF > /tmp/test1-output
put file.270 /dir1
put file.270 /dir5/
put file.270 file.2701
put file.270 file.2702
put file.270 file.2703
ls
quit
EOF

cmp /tmp/test1-output <<EOF || { echo TEST q2.2 FAILED; exit; }
read/write block size: 1000
cmd> put file.270 /dir1
error: File exists
cmd> put file.270 /dir5/
error: No such file or directory
cmd> put file.270 file.2701
cmd> put file.270 file.2702
error: No space left on device
cmd> put file.270 file.2703
error: No space left on device
cmd> ls
dir1
file.270
file.2701
file.2702
file.2703
file.7
file.A
cmd> quit
EOF

if [ $? -eq 0 ]; then
	echo "Test q2.2 passed"
else
	echo "Test q2.2 failed"
fi

#---------------------------------------------------------------------
# TEST CASE# 3 : To test mkdir when there is no space and for -ve case
#---------------------------------------------------------------------
./homework -cmdline -image disk.img <<EOF > /tmp/test1-output
mkdir dir5/dir6
mkdir dir5
quit
EOF

cmp /tmp/test1-output <<EOF || { echo TEST q2.3 FAILED; exit; }
read/write block size: 1000
cmd> mkdir dir5/dir6
error: No such file or directory
cmd> mkdir dir5
error: No space left on device
cmd> quit
EOF

if [ $? -eq 0 ]; then
	echo "Test q2.3 passed"
else
	echo "Test q2.3 failed"
fi

# creating a new image file
rm -rf disk.img
./mktest disk.img

#-------------------------------------------------------------------
# TEST CASE# 4 : To check for invalid path while writing a file
#-------------------------------------------------------------------
./homework -cmdline -image disk.img <<EOF > /tmp/test1-output
put file.270 /dir5/file.270
quit
EOF

cmp /tmp/test1-output <<EOF || { echo TEST q2.4 FAILED; exit; }
read/write block size: 1000
cmd> put file.270 /dir5/file.270
error: No such file or directory
cmd> quit
EOF

if [ $? -eq 0 ]; then
	echo "Test q2.4 passed"
else
	echo "Test q2.4 failed"
fi

# creating a new image file
rm -rf disk.img
./mktest disk.img

#-------------------------------------------------------------------
# TEST CASE# 5 : To check unlink and truncate for negative cases
#-------------------------------------------------------------------
./homework -cmdline -image disk.img <<EOF > /tmp/test1-output
rm file.270
rm dir1
rm /dir2/file.0
quit
EOF

cmp /tmp/test1-output <<EOF || { echo TEST q2.5 FAILED; exit; }
read/write block size: 1000
cmd> rm file.270
error: No such file or directory
cmd> rm dir1
error: Is a directory
cmd> rm /dir2/file.0
error: No such file or directory
cmd> quit
EOF

if [ $? -eq 0 ]; then
	echo "Test q2.5 passed"
else
	echo "Test q2.5 failed"
fi

#-------------------------------------------------------------------
# TEST CASE# 6 : To check unlink and truncate for positive cases
#-------------------------------------------------------------------
./homework -cmdline -image disk.img <<EOF > /tmp/test1-output
rm /dir1/file.270
quit
EOF

cmp /tmp/test1-output <<EOF || { echo TEST q2.6 FAILED; exit; }
read/write block size: 1000
cmd> rm /dir1/file.270
cmd> quit
EOF

if [ $? -eq 0 ]; then
	echo "Test q2.6 passed"
else
	echo "Test q2.6 failed"
fi

#-------------------------------------------------------------------
# TEST CASE# 7 : To test statfs
#-------------------------------------------------------------------
./homework -cmdline -image disk.img <<EOF > /tmp/test1-output
statfs
quit
EOF

cmp /tmp/test1-output <<EOF || { echo TEST q2.7 FAILED; exit; }
read/write block size: 1000
cmd> statfs
max name length: 27
block size: 1024
cmd> quit
EOF

if [ $? -eq 0 ]; then
	echo "Test q2.7 passed"
else
	echo "Test q2.7 failed"
fi

#---------------------------------------------------
# TEST CASE# 8 : To test truncate for negative case
#---------------------------------------------------
./homework -cmdline -image disk.img <<EOF > /tmp/test1-output
truncate /dir1
truncate /dir2/file.A
quit
EOF

cmp /tmp/test1-output <<EOF || { echo TEST q2.8 FAILED; exit; }
read/write block size: 1000
cmd> truncate /dir1
error: Is a directory
cmd> truncate /dir2/file.A
error: No such file or directory
cmd> quit
EOF

if [ $? -eq 0 ]; then
	echo "Test q2.8 passed"
else
	echo "Test q2.8 failed"
fi

#---------------------------------------------------
# TEST CASE# 9 : To test truncate for positive cases
#---------------------------------------------------
./homework -cmdline -image disk.img <<EOF > /tmp/test1-output
truncate file.7
truncate file.A
truncate /dir1/file.270
ls
cd dir1
ls
quit
EOF

cmp /tmp/test1-output <<EOF || { echo TEST q2.9 FAILED; exit; }
read/write block size: 1000
cmd> truncate file.7
cmd> truncate file.A
cmd> truncate /dir1/file.270
error: No such file or directory
cmd> ls
dir1
file.7
file.A
cmd> cd dir1
cmd> ls
file.0
file.2
cmd> quit
EOF

if [ $? -eq 0 ]; then
	echo "Test q2.9 passed"
else
	echo "Test q2.9 failed"
fi

./mkfs-x6 -size 50m big.img

cat file.270 >> largeFile.270
cat file.270 >> largeFile.270
cat file.270 >> largeFile.270

#----------------------------------------------------
# TEST CASE# 10 : To test write for a very large file
#----------------------------------------------------
./homework -cmdline -image big.img <<EOF > /tmp/test1-output
put largeFile.270
ls
quit
EOF

cmp /tmp/test1-output <<EOF || { echo TEST q2.10 FAILED; exit; }
read/write block size: 1000
cmd> put largeFile.270
cmd> ls
largeFile.270
cmd> quit
EOF

if [ $? -eq 0 ]; then
	echo "Test q2.10 passed"
else
	echo "Test q2.10 failed"
fi

#-----------------------------------------
# TEST CASE# 11 : To test utime for a file
#-----------------------------------------
./homework -cmdline -image disk.img <<EOF > /tmp/test1-output
ls-l file.7
quit
EOF

./homework -cmdline -image disk.img <<EOF > /tmp/test1.11-output
utime file.7
ls-l file.7
quit
EOF

date1=$(grep '/file.7' /tmp/test1-output | cut -d ' ' -f6-9)
date2=$(grep '/file.7' /tmp/test1.11-output | cut -d ' ' -f6-9)

epochdate1=$(date -d "$date1" +"%s")
epochdate2=$(date -d "$date2" +"%s")

if [ $epochdate1 -le $epochdate2 ]; then
	echo "Test q2.11 passed"
else
	echo "Test q2.11 failed"
fi
