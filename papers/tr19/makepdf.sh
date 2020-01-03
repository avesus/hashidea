#!/bin/bash

function usage()
{
    echo "makepdf.sh [-b][-v] <texname>"
    exit -1
}

function verbmsg()
{
    if [ $verbose == 1 ]; then
	echo $*
    fi
}

function runpdf()
{
    pdflatex -halt-on-error $1  > /tmp/$$.output 2>&1
    if [ $? != 0 ]; then
	cat /tmp/$$.output
	echo "Error running pdflatex $2 on $1."
	exit -1
    fi
}

################################################################
# main entry point

runbib=0
verbose=0
while [ "$#" -gt "0" ]
do
    if [ ${1#-} == $1 ]; then
	break;
    fi
    opt=$1;
    shift;
    case $opt in
	-v)
	    verbose=1
	    ;;
	-b)
	    runbib=1
	    ;;
	*)
	    echo "Unknown Option: $opt"
	    usage
    esac
done

if [ "$#" -ne 1 ]; then
    echo "makepdf.sh [-b][-v] <texname>"
    exit -1
fi
rm -f *.log *.aux *.out
basename=${1%%.tex}
texfile=${1%%.tex}.tex
logfile=${1%%.tex}.log
auxfile=${1%%.tex}.aux
bblfile=${1%%.tex}.bbl
blgfile=${1%%.tex}.blg

if [ ! -e $texfile ]; then
    echo "$texfile not a tex file?"
    exit -1
fi

runpdf $1 "first time"

if [ ! -e $logfile ]; then
    cat /tmp/$$.output
    echo "Could not find the logfile: $logfile"
    exit -1
fi
grep "Output written on" /tmp/$$.output
if [ $runbib -eq 1 ] && [ -e $auxfile ]; then
    # we want a bib processed, see if we need to
    grep -q "[\]citation" $auxfile > /dev/null
    if [ $? == 1 ]; then
	# there were no citations in aux file, so we can skip running bibtex
	verbmsg "No citations, so no need to run bibtex"
	runbib=0
    fi
    if [ $runbib -eq 1 ] && [ -e $blgfile ]; then
	runbib=0
	for bfile in `grep "Database file" *.blg | sed -e 's/.*:\s*//'`; do
	    if [ $bfile -nt $bblfile ]; then
		# at least one .bib file is newer than the bbl file so we need to run bibtex
		verbmsg "$bfile is newer than $bblfile, need to run bibtex"
		runbib=1
		break
	    fi
	done
    fi
    if [ $runbib -eq 1 ]; then
	bibtex $basename
	runpdf $1 "after bibtex"
    fi
fi    
grep -q "undefined references\|Rerun to get cross-references right" $logfile > /dev/null
if [ $? -eq 0 ]; then
    echo "Rerunning to get labels right"
    runpdf $1 "second time"
    grep -q "undefined references\|Rerun to get cross-references right" $logfile > /dev/null
    if [ $? -eq 0 ]; then
	echo "There are still undefined references or references changed."
	grep undefined $logfile
    fi
fi
