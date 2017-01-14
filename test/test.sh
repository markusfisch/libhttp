#!/bin/bash

# Log URL
#
# @param 1 - URL
log() {
	echo $1 >> failed
}

# Test URL
#
# @param 1 - URL
test_url() {
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
www.apple.com \
hhsw.de \
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
