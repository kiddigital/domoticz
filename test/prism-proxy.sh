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

echo -n "Starting Proxy services..."

${prm} proxy -h 10.0.2.15 ../OpenAPI/domoticz.openapi_v2.yml http://localhost:8080 &
sleep 1
${lhd} -D -f lighttpd-proxy.conf &
sleep 1

echo "started"

while :
do
	sleep 10
done

echo "Done"
