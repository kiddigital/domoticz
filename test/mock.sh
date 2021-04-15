#!/bin/bash

lhd=`which lighttpd`
prm=`which prism`

trap ctrl_c INT

ctrl_c() {
	echo -n "Trying to stop services..."

	#kill `ps h -C lighttpd -o pid`
	sleep 1
	#kill `ps h -C prism -o pid`

	echo "Stopped"
	exit
}

echo -n "Checking webroot link to fake data"

if [ ! -d "../www/faked" ]; then
	ln -s ../test/faked ../www/faked
	echo "created"
elif
	echo "already exists"
fi

echo -n "Starting Mock services..."

${prm} mock -h 10.0.2.15 ../OpenAPI/domoticz.openapi_v2.yml &
sleep 1
${lhd} -D -f lighttpd.conf &
sleep 1

echo "started"

while :
do
	sleep 10
done

echo "Done"
