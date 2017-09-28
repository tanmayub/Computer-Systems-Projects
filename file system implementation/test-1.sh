#!/bin/bash

# Bash script to test question 3 of homework 1
# Usage :- chmod +x test-1.sh
#          ./test-1.sh

# run make and make clean and also create a new disk image

make clean > /dev/null
rm *.gcno *.gcda
make COVERAGE=1 > /dev/null
rm -rf disk.img
./mktest disk.img

#-------------------------------------------------------------------
# TEST CASE# 1 : To check that basic ls and ls-l works i.e. command 
#                does not fail. 
#-------------------------------------------------------------------
./homework -cmdline -image disk.img <<EOF > /tmp/test1-output
ls
ls-l
ls file.A
quit
EOF

cmp /tmp/test1-output <<EOF || { echo TEST q1.1 FAILED; exit; }
read/write block size: 1000
cmd> ls
dir1
file.7
file.A
cmd> ls-l
dir1 drwxr-xr-x 0 1 Fri Jul 13 07:08:00 2012
file.7 -rwxrwxrwx 6644 7 Fri Jul 13 07:06:20 2012
file.A -rwxrwxrwx 1000 1 Fri Jul 13 07:04:40 2012
cmd> ls file.A
/file.A
cmd> quit
EOF
if [ $? -eq 0 ]; then
	echo "Test q1.1 passed"
else
	echo "Test q1.1 failed"
fi
#---------------------------------------------------------------------
# TEST CASE# 2 : To test whether ls-l works for all the negative cases
#---------------------------------------------------------------------

./homework -cmdline -image disk.img <<EOF > /tmp/test1-output
ls dir2
ls-l /dir1/file.270/
quit
EOF

cmp /tmp/test1-output <<EOF || { echo TEST q1.2 FAILED; exit; }
read/write block size: 1000
cmd> ls dir2
error: No such file or directory
cmd> ls-l /dir1/file.270/
error: Not a directory
cmd> quit
EOF
if [ $? -eq 0 ]; then
	echo "Test q1.2 passed"
else
	echo "Test q1.2 failed"
fi
#---------------------------------------------------------------------
# TEST CASE# 3 : To test whether mkdir works for positive cases
#---------------------------------------------------------------------

./homework -cmdline -image disk.img <<EOF > /tmp/test1-output
mkdir dir2
ls
mkdir dir2/dir3
cd dir2
ls
quit
EOF

cmp /tmp/test1-output <<EOF || { echo TEST q1.3 FAILED; exit; }
read/write block size: 1000
cmd> mkdir dir2
cmd> ls
dir1
dir2
file.7
file.A
cmd> mkdir dir2/dir3
cmd> cd dir2
cmd> ls
dir3
cmd> quit
EOF
if [ $? -eq 0 ]; then
	echo "Test q1.3 passed"
else
	echo "Test q1.3 failed"
fi
#------------------------------------------------
# TEST CASE# 4 : To test mkdir for error cases
#------------------------------------------------
./homework -cmdline -image disk.img <<EOF > /tmp/test1-output
mkdir dir2
quit
EOF

cmp /tmp/test1-output <<EOF || { echo TEST q1.3 FAILED; exit; }
read/write block size: 1000
cmd> mkdir dir2
error: File exists
cmd> quit
EOF
if [ $? -eq 0 ]; then
	echo "Test q1.4 passed"
else
	echo "Test q1.4 failed"
fi

#------------------------------------------------
# TEST CASE# 5 : To test rmdir for error cases
#------------------------------------------------
./homework -cmdline -image disk.img <<EOF > /tmp/test1-output
rmdir dir2
rmdir dir3
rmdir file.A
quit
EOF

cmp /tmp/test1-output <<EOF || { echo TEST q1.3 FAILED; exit; }
read/write block size: 1000
cmd> rmdir dir2
error: Directory not empty
cmd> rmdir dir3
error: No such file or directory
cmd> rmdir file.A
error: Not a directory
cmd> quit
EOF

if [ $? -eq 0 ]; then
	echo "Test q1.5 passed"
else
	echo "Test q1.5 failed"
fi

#------------------------------------------------
# TEST CASE# 6 : To test rmdir for positive cases
#------------------------------------------------
./homework -cmdline -image disk.img <<EOF > /tmp/test1-output
rmdir dir2/dir3
rmdir dir2
ls
quit
EOF

cmp /tmp/test1-output <<EOF || { echo TEST q1.3 FAILED; exit; }
read/write block size: 1000
cmd> rmdir dir2/dir3
cmd> rmdir dir2
cmd> ls
dir1
file.7
file.A
cmd> quit
EOF

if [ $? -eq 0 ]; then
	echo "Test q1.6 passed"
else
	echo "Test q1.6 failed"
fi

#------------------------------------------------
# TEST CASE# 7 : To test chmod for positive cases
#                and negative cases
#------------------------------------------------
./homework -cmdline -image disk.img <<EOF > /tmp/test1-output
chmod 755 dir1
chmod 777 file.7
chmod 777 file.88
ls-l
quit
EOF

cmp /tmp/test1-output <<EOF || { echo TEST q1.3 FAILED; exit; }
read/write block size: 1000
cmd> chmod 755 dir1
cmd> chmod 777 file.7
cmd> chmod 777 file.88
error: No such file or directory
cmd> ls-l
dir1 drwxr-xr-x 0 1 Fri Jul 13 07:08:00 2012
file.7 -rwxrwxrwx 6644 7 Fri Jul 13 07:06:20 2012
file.A -rwxrwxrwx 1000 1 Fri Jul 13 07:04:40 2012
cmd> quit
EOF

if [ $? -eq 0 ]; then
	echo "Test q1.7 passed"
else
	echo "Test q1.7 failed"
fi

#------------------------------------------------
# TEST CASE# 8 : To test rename for error cases
#------------------------------------------------
./homework -cmdline -image disk.img <<EOF > /tmp/test1-output
rename file.A file.7
rename file.23 file.27
rename /dir1/file.270 file.7
quit
EOF

cmp /tmp/test1-output <<EOF || { echo TEST q1.3 FAILED; exit; }
read/write block size: 1000
cmd> rename file.A file.7
error: File exists
cmd> rename file.23 file.27
error: No such file or directory
cmd> rename /dir1/file.270 file.7
error: Invalid argument
cmd> quit
EOF

if [ $? -eq 0 ]; then
	echo "Test q1.8 passed"
else
	echo "Test q1.8 failed"
fi

#------------------------------------------------
# TEST CASE# 9 : To test rename for positive cases
#------------------------------------------------
./homework -cmdline -image disk.img <<EOF > /tmp/test1-output
rename dir1 dir2
rename file.7 file.7
rename file.A file.B
ls
quit
EOF

cmp /tmp/test1-output <<EOF || { echo TEST q1.3 FAILED; exit; }
read/write block size: 1000
cmd> rename dir1 dir2
cmd> rename file.7 file.7
cmd> rename file.A file.B
cmd> ls
dir2
file.7
file.B
cmd> quit
EOF

if [ $? -eq 0 ]; then
	echo "Test q1.9 passed"
else
	echo "Test q1.9 failed"
fi
