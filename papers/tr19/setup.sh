#!/bin/bash
for x in ../../bib/references.bib  ../styles/myifdefs.tex  ../styles/defs.sty ../styles/figs.tex ../styles/time.tex; do
    y=`basename $x`
    if [ ! -e $y ]; then
	echo "Added link for $y"
	ln -s $x .
    fi
    grep -q $y .gitignore
    if [ $? == 1 ]; then
	echo "Added $y to gitignore"
	echo $y >> .gitignore
	git commit -m "added linked tex file $y" .gitignore
    fi
done

