AC_PREREQ([2.69])
AC_INIT([streamproxy], [0.0], [erik@openpli.org])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])

AC_USE_SYSTEM_EXTENSIONS
AC_SYS_LARGEFILE
AC_GNU_SOURCE

AC_PROG_CXX
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile src/Makefile])
AC_CHECK_LIB([boost_program_options], [boost_program_options::notify])
AC_CHECK_HEADERS([fcntl.h netdb.h netinet/in.h stddef.h stdint.h stdlib.h string.h sys/ioctl.h sys/socket.h syslog.h unistd.h shadow.h])
AC_CHECK_HEADER_STDBOOL

AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT8_T

AC_FUNC_FORK
AC_CHECK_FUNCS([socket strerror strtoul])
AC_CHECK_FUNCS([getspnam socket strchr strerror strtol strtoul])

AC_OUTPUT
