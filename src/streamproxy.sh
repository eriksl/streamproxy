### BEGIN INIT INFO
# Provides:				streamproxy
# Required-Start:		$syslog $networking
# Required-Stop:		$syslog
# Default-Start:		2 3 4 5
# Default-Stop:			1
# Short-Description:	Streamproxy for streaming and transcoding live and files.
### END INIT INFO

PATH=/sbin:/bin:/usr/sbin:/usr/bin
DAEMON=/usr/bin/streamproxy
NAME=streamproxy
DESC="Streamproxy"

test -x "$DAEMON" || exit 0

case "$1" in

	start)
		echo -n "Starting ${DESC}: "
		start-stop-daemon -S -x "$DAEMON"
		echo "${NAME}."
		;;

	stop)
		echo -n "Stopping ${DESC}: "
		start-stop-daemon -K -x "$DAEMON"
		echo "${NAME}."
		;;

	*)
		echo "usage: /etc/init.d/${NAME} {start|stop}" >&2
		exit 1
		;;
esac

exit 0
