#!/bin/bash

# Bash script to test question 3 of homework 1
# Usage :- chmod +x test-2.sh
#          ./test-2.sh

# run make and make clean and also create a new disk image
rm -rf disk.img
./mktest disk.img
rm -rf dir
mkdir dir

#-----------------------------
# Mount the file system on dir
# ----------------------------

./homework -image disk.img ./dir

cd dir

#-------------------------------------------
# TEST CASE# 1 : To test ls and ls-l in FUSE
#-------------------------------------------
ls > /tmp/test3-output
ls -l >> /tmp/test3-output

cmp /tmp/test3-output <<EOF || { echo TEST q3.1 FAILED; exit; }
dir1
file.7
file.A
total 5
drwxr-xr-x 0 student student    0 Jul 13  2012 dir1
-rwxrwxrwx 0 student student 6644 Jul 13  2012 file.7
-rwxrwxrwx 0 student student 1000 Jul 13  2012 file.A
EOF

if [ $? -eq 0 ]; then
	echo "Test q3.1 passed"
else
	echo "Test q3.1 failed"
fi

#-----------------------------------------------
# TEST CASE# 2 : To test mkdir and rmdir in FUSE
#-----------------------------------------------
mkdir dir1 > /tmp/test3-output 2>&1
mkdir dir2 >> /tmp/test3-output 2>&1 
cd dir2 >> /tmp/test3-output 2>&1
mkdir dir3 >> /tmp/test3-output 2>&1
cd dir3 >> /tmp/test3-output 2>&1
cd ../.. >> /tmp/test3-output 2>&1
rmdir dir2 >> /tmp/test3-output 2>&1
cd dir2 >> /tmp/test3-output 2>&1
rmdir dir3 >> /tmp/test3-output 2>&1
rmdir file.A >> /tmp/test3-output 2>&1
cd .. >> /tmp/test3-output 2>&1
rmdir dir2 >> /tmp/test3-output 2>&1
rmdir file.7 >> /tmp/test3-output 2>&1

cmp /tmp/test3-output <<EOF || { echo TEST q3.2 FAILED; exit; }
mkdir: cannot create directory ‘dir1’: File exists
rmdir: failed to remove ‘dir2’: Directory not empty
rmdir: failed to remove ‘file.A’: No such file or directory
rmdir: failed to remove ‘file.7’: Not a directory
EOF

if [ $? -eq 0 ]; then
	echo "Test q3.2 passed"
else
	echo "Test q3.2 failed"
fi

#-------------------------------------
# TEST CASE# 3 : To test chmod in FUSE
#-------------------------------------

chmod 644 dir1 > /tmp/test3-output 2>&1
chmod 644 file.A >> /tmp/test3-output 2>&1
chmod u+x dir1 >> /tmp/test3-output 2>&1
chmod 755 abc >> /tmp/test3-output 2>&1

cmp /tmp/test3-output <<EOF || { echo TEST q3.2 FAILED; exit; }
chmod: cannot access ‘abc’: No such file or directory
EOF

if [ $? -eq 0 ]; then
	echo "Test q3.3 passed"
else
	echo "Test q3.3 failed"
fi

#-------------------------------------
# TEST CASE# 4 : To test rename in FUSE
#-------------------------------------

mv file.7 file.7 > /tmp/test3-output 2>&1
mv file.7 file.A >> /tmp/test3-output 2>&1
mv file.7 file.8 >> /tmp/test3-output 2>&1
mv /dir1/file.270 file.9 >> /tmp/test3-output 2>&1
mv file.88 file.9 >> /tmp/test3-output 2>&1

cmp /tmp/test3-output <<EOF || { echo TEST q3.2 FAILED; exit; }
mv: ‘file.7’ and ‘file.7’ are the same file
mv: cannot move ‘file.7’ to ‘file.A’: File exists
mv: cannot stat ‘/dir1/file.270’: No such file or directory
mv: cannot stat ‘file.88’: No such file or directory
EOF

if [ $? -eq 0 ]; then
	echo "Test q3.4 passed"
else
	echo "Test q3.4 failed"
fi

#------------------------------------------
# TEST CASE# 5 : To test read/write in FUSE
#------------------------------------------

cat dir1/file.270 > /tmp/test3-output 2>&1
cp dir1/file.270 /tmp/test3.5-output

cmp /tmp/test3-output /tmp/test3.5-output || { echo TEST q3.5 FAILED; exit; }

if [ $? -eq 0 ]; then
	echo "Test q3.5 passed"
else
	echo "Test q3.5 failed"
fi

rm -f /tmp/test3.5-output

#----------------------------------------
# TEST CASE# 6 : To test truncate in FUSE
#----------------------------------------

truncate --size 0 dir1/file.2 > /tmp/test3-output 2>&1
touch /tmp/test3.5-output
cp dir1/file.2 /tmp/

cmp /tmp/file.2 /tmp/test3.5-output || { echo TEST q3.6 FAILED; exit; }

if [ $? -eq 0 ]; then
	echo "Test q3.6 passed"
else
	echo "Test q3.6 failed"
fi

rm -f /tmp/test3.5-output

#-----------------------------------------------------
# TEST CASE# 7 : To test truncate for -ve case in FUSE
#-----------------------------------------------------

truncate --size 0 dir1 > /tmp/test3-output 2>&1

cmp /tmp/test3-output <<EOF || { echo TEST q3.7 FAILED; exit; }
truncate: cannot open ‘dir1’ for writing: Is a directory
EOF
if [ $? -eq 0 ]; then
	echo "Test q3.7 passed"
else
	echo "Test q3.7 failed"
fi

#-------------------------------------------
# TEST CASE# 8 : To test remove file in FUSE
#-------------------------------------------

rm dir1 > /tmp/test3-output 2>&1
rm dir1/file.270 >> /tmp/test3-output 2>&1
rm file.11 >> /tmp/test3-output 2>&1

cmp /tmp/test3-output <<EOF || { echo TEST q3.8 FAILED; exit; }
rm: cannot remove ‘dir1’: Is a directory
rm: cannot remove ‘file.11’: No such file or directory
EOF

if [ $? -eq 0 ]; then
	echo "Test q3.8 passed"
else
	echo "Test q3.8 failed"
fi

cd ..
fusermount -u dir
