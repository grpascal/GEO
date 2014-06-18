#!/bin/bash

rm -rf html
svn up html
doxygen libmxcam_doxygen.cfg
if [ $? -eq 127 ] ; then
	echo "doxygen command is not installed on your system.install doxygen before running this script"
	exit;
fi	

echo "----------------------------------------"
echo "list of deleted files"
echo "----------------------------------------"
del_list=`diff -aur --exclude .svn --brief html new_html/ | grep "Only in html" | cut -d : -f 2 ` ;

for f in ${del_list}
do
	echo ${f}
	svn rm --force  html/${f}
	rm -rf html/${f}
done
echo "----------------------------------------"
echo "list of added files"
echo "----------------------------------------"
add_list=`diff -aur --exclude .svn --brief html new_html/ | grep "Only in new_html" | cut -d : -f 2 `; 
for f in ${add_list}
do

	echo ${f}
	cp new_html/${f} html
	svn add --force  html/${f}
done
echo "----------------------------------------"
echo "list of modified files"
echo "----------------------------------------"
copy_list=`diff -aur --exclude .svn --brief html new_html/ | grep "differ" | cut -d " " -f 4 `;
echo "Copy list ${copy_list}"
for f in ${copy_list}
do
	echo ${f}
	cp ${f} html/
done
rm -rf new_html/
