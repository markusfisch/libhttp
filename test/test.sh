#!/bin/bash

# Log URL
#
# @param 1 - URL
log()
{
	echo $1 >> failed
}

# Test URL
#
# @param 1 - URL
test_url()
{
	local URL=$1

	echo -n "Checking $URL ... "

	curl --user-agent 'Mozilla/4.0 (Linux)' -s $URL > $CURL_OUT || {
		echo "skipped (curl did return $?)"
		continue
	}

	./http $URL > $HTTP_OUT || {
		echo "skipped (http did return $?)"
		continue
	}

	curl --user-agent 'Mozilla/4.0 (Linux)' -s $URL |
		diff -q - $CURL_OUT > /dev/null || {
		echo "skipped (first curl request doesn't match second)"
		continue
	}

	if diff -q $CURL_OUT $HTTP_OUT > /dev/null
	then
		echo 'success'
	elif (( LOGALL ))
	then
		echo 'FAILED! (logged)'
		log $URL
	else
		echo -n 'FAILED! View diff? ([y]|n|log|all) '
		read
		case $REPLY in
			[l]*)
				log $URL
				;;
			[a]*)
				LOGALL=1
				log $URL
				;;
			[nN]*|[qQ]*)
				;;
			*)
				diff $CURL_OUT $HTTP_OUT | less
				;;
		esac
	fi

	rm $CURL_OUT $HTTP_OUT
}

readonly CURL_OUT=${CURL_OUT:-'curl-out'}
readonly HTTP_OUT=${HTTP_OUT:-'http-out'}

LOGALL=${LOGALL:-0}

(cd .. && make clean && make) &&
	make clean && make &&
	for URL in ${@:-\
www.blender.org/ \
inkscape.org/ \
www.gimp.org/ \
en.wikipedia.org/wiki/Main_Page \
en.wikipedia.org/wiki/HTTP \
en.wikipedia.org/wiki/List_of_Advanced_Dungeons_%26_Dragons_2nd_edition_monsters \
www.mozilla.org/de/about/ \
www.cgsociety.org/ \
www.wired.com \
developer.android.com/index.html \
www.ibm.com/us/en/ \
www.adobe.com \
www.apple.com \
www.samsung.com/de/ \
www.microsoft.com/en-us/default.aspx \
www.motogp.com \
stackoverflow.com \
square.github.io/dagger/ \
www.spiegel.de \
www.golem.de \
www.welt.de \
www.chip.de \
www.kicker.de \
markusfisch.de}
do
	if [ -r $URL ]
	then
		while read <&3
		do
			test_url $REPLY
		done 3< $URL
	else
		test_url $URL
	fi
done
