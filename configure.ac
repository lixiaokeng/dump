AC_INIT([dump], [0.4b45], [http://dump.sourceforge.net])
AC_PREREQ(2.57)
AM_INIT_AUTOMAKE
AM_SILENT_RULES([yes])
AC_CONFIG_MACRO_DIR([m4])

AC_CONFIG_HEADER(config.h)

dnl
dnl Check for programs
dnl
AC_PROG_MAKE_SET
AC_PROG_LN_S
PKG_PROG_PKG_CONFIG
AC_PROG_CC
AC_PROG_INSTALL

AC_USE_SYSTEM_EXTENSIONS
AC_SYS_LARGEFILE

dnl Has to be after AC_PROG_CC and other core compiler checks.
LT_INIT

AC_CHECK_HEADERS([sys/types.h])

CPPFLAGS="${CPPFLAGS} -D_USE_BSD_SIGNAL "'-D_PATH_DUMPDATES=\"$(DUMPDATESPATH)\" -D_DUMP_VERSION=\"$(VERSION)\"'

AC_DEFINE([LINUX_FORK_BUG], 1, [Workaround bad fork behavior on Linux])

dnl Should these be options?
AC_DEFINE([RDUMP], 1, [Enable rdump support])
AC_DEFINE([RRESTORE], 1, [Enable rrestore support])

dnl
dnl Handle --enable-debug
dnl
AC_MSG_CHECKING([whether to enable extra debugging])
AC_ARG_ENABLE([debug], [AC_HELP_STRING([--enable-debug], [include debugging code @<:@default=no@:>@])])
if test "$enable_debug" = "yes"
then
	AC_DEFINE([FDEBUG], 1, [Enable extra debugging])
	AC_DEFINE([TDEBUG], 1, [Enable extra debugging])
	AC_DEFINE([WRITEDEBUG], 1, [Enable extra debugging])
	AC_DEFINE([DIRDEBUG], 1, [Enable extra debugging])
	AC_MSG_RESULT([yes])
else
	AC_MSG_RESULT([no])
fi

dnl
dnl Handle --enable-static-progs
dnl
AC_MSG_CHECKING([whether to build static binaries])
AC_ARG_ENABLE([static-progs], [AC_HELP_STRING([--enable-static-progs], [link dump and restore statically @<:@default=no@:>@])])
if test "$enable_static_progs" = "yes"
then
	LDFLAGS="$LDFLAGS -static"
	PKG_CONFIG="$PKG_CONFIG --static"
	AC_MSG_RESULT([yes])
else
	AC_MSG_RESULT([no])
fi

dnl
dnl Handle --enable-rmt
dnl
AC_MSG_CHECKING([whether to build rmt])
BUILD_RMT=yes
AC_ARG_ENABLE([rmt], [AC_HELP_STRING([--enable-rmt], [compile and install rmt @<:@default=yes@:>@])])
if test "$enable_rmt" = "no"; then
	BUILD_RMT=no
fi
AM_CONDITIONAL([BUILD_RMT], [test "$BUILD_RMT" = "yes"])
AC_MSG_RESULT([$BUILD_RMT])

dnl
dnl Handle --enable-ermt
dnl
AC_MSG_CHECKING([whether to build ermt])
BUILD_ERMT=no
AC_ARG_ENABLE([ermt], [AC_HELP_STRING([--enable-ermt], [compile ermt, an encrypting version of rmt @<:@default=no@:>@])])
if test "$enable_ermt" = "yes"; then
	if test "$BUILD_RMT" != "yes"; then
		AC_MSG_ERROR([ermt requires --enable-rmt])
	fi
	BUILD_ERMT=yes
fi
AM_CONDITIONAL([BUILD_ERMT], [test "$BUILD_ERMT" = "yes"])
AC_MSG_RESULT([$BUILD_ERMT])

dnl
dnl Handle --enable-kerberos
dnl
AC_MSG_CHECKING([whether to enable kerberos extensions])
AC_ARG_ENABLE([kerberos], [AC_HELP_STRING([--enable-kerberos], [compile kerberos extensions @<:@default=no@:>@])])
if test "$enable_kerberos" = "yes"
then
	AC_DEFINE([KERBEROS], 1, [Enable kerberos extensions])
	AC_MSG_RESULT([yes])
else
	AC_MSG_RESULT([no])
fi

dnl
dnl Handle --enable-readline
dnl
AC_ARG_ENABLE([readline], [AC_HELP_STRING([--enable-readline], [enable readline support in restore @<:@default=yes@:>@])],
if test "$enableval" = "no"
then
	READLINE=""
	echo "Not including readline support"
else
	READLINE="yes"
	AC_DEFINE([HAVE_READLINE],1,[Define if you want to include readline support.])
	echo "Including readline support"
fi
,
READLINE="yes"
AC_DEFINE([HAVE_READLINE],1,[Define if you want to include readline support.])
echo "Including readline support by default"
)

dnl
dnl Handle --enable-oldsylefscript
dnl
AC_ARG_ENABLE([oldstylefscript], [AC_HELP_STRING([--enable-oldstylefscript], [enable old style F script (no arguments) @<:@default=no@:>@])],
if test "$enableval" = "yes"
then
	AC_DEFINE([OLD_STYLE_FSCRIPT],1,[Define this is you want old style F script (no arguments).])
	echo "Using old style F script"
else
	echo "Using new style F script"
fi
,
echo "Using new style F script by default"
)

dnl
dnl Handle --enable-qfa
dnl
AC_ARG_ENABLE([qfa], [AC_HELP_STRING([--enable-qfa], [enable Quick File Access support @<:@default=yes@:>@])],
if test "$enableval" = "yes"
then
	AC_DEFINE([USE_QFA],1,[Define this if you want Quick File Access support.])
	echo "Enabling Quick File Access support"
else
	echo "Not enabling Quick File Access support"
fi
,
AC_DEFINE([USE_QFA],1,[Define this if you want Quick File Access support.])
echo "Enabling Quick File Access support by default"
)

dnl
dnl Handle --enable-qfadebug
dnl
AC_ARG_ENABLE([qfadebug], [AC_HELP_STRING([--enable-qfadebug], [include Quick File Access debugging code @<:@default=no@:>@])],
if test "$enableval" = "yes"
then
	AC_DEFINE([DEBUG_QFA],1,[Define this if you want to include Quick File Access debugging code.])
	echo "Including Quick File Access debugging code"
else
	echo "Not including Quick File Access debugging code"
fi
,
echo "Not including Quick File Access debugging code by default"
)

dnl
dnl Handle --enable-macosx
dnl
AC_ARG_ENABLE([macosx], [AC_HELP_STRING([--enable-macosx], [include Mac OSX restore compatibility @<:@default=no@:>@])],
if test "$enableval" = "yes"
then
	AC_DEFINE([DUMP_MACOSX],1,[Define this if you want to include Mac OSX restore compatibility.])
	echo "Including Mac OSX restore compatibility code"
else
	echo "Not including Mac OSX restore compatibility code"
fi
,
echo "Not including Mac OSX restore compatibility code by default"
)

dnl
dnl Handle --enable-selinux
dnl
AC_ARG_ENABLE([selinux], [AC_HELP_STRING([--enable-selinux], [restore can translate SELinux EAs @<:@default=auto@:>@])])
if test "$enable_selinux" != "no"; then
	found_selinux="yes"
	PKG_CHECK_MODULES([SELINUX], [libselinux], [], [
		found_selinux="no"
		if test "$enable_selinux" = "yes"; then
			AC_MSG_ERROR([SELinux support requested, but not found])
		fi
	])
	if test "$found_selinux" != "no"; then
		AC_DEFINE([TRANSSELINUX], 1, [Support SELinux labels])
	fi
fi

dnl
dnl set $(DUMPDATESPATH) from --with-dumpdatespath
dnl
AC_ARG_WITH([dumpdatespath], [AC_HELP_STRING([--with-dumpdatespath=PATH], [select path for dumpdates file])],
AC_MSG_RESULT(DUMPDATESPATH is $withval)
DUMPDATESPATH=$withval,
DUMPDATESPATH="${sysconfdir}/dumpdates"
echo "DUMPDATESPATH defaults to $DUMPDATESPATH"
)dnl
AC_SUBST(DUMPDATESPATH)

dnl
dnl set _PATH_RMT from --with-rmtpath
dnl
AC_ARG_WITH([rmtpath], [AC_HELP_STRING([--with-rmtpath=PATH], [select path for rmt program])],
RMTPATH=$withval,
RMTPATH='/etc/rmt')
AC_SUBST(RMTPATH)
CPPFLAGS="${CPPFLAGS} "'-D_PATH_RMT=\"$(RMTPATH)\"'

dnl
dnl Check for Ext2fs headers and libraries
dnl
PKG_CHECK_MODULES(EXT2FS, [ext2fs])
PKG_CHECK_MODULES(COM_ERR, [com_err])

dnl
dnl Check for ext2fs_read_inode_full
dnl
AC_CHECK_LIB(ext2fs, ext2fs_read_inode_full, [rif=yes], [rif=no])
if test "$rif" = yes; then
	AC_DEFINE([HAVE_EXT2FS_READ_INODE_FULL],1,[Define this if your ext2fs libs have the ext2fs_read_inode_full function.])
fi

dnl
dnl Try to use ext2_fs.h header from libext2fs instead of from the kernel
dnl
AC_CHECK_HEADERS(ext2fs/ext2_fs.h, [], [], [-])

dnl
dnl Check for ext2_ino_t type
dnl
AC_MSG_CHECKING(for ext2_ino_t type in libext2fs headers)
AC_TRY_COMPILE([#include <stdio.h>
#ifdef HAVE_EXT2FS_EXT2_FS_H
#include <ext2fs/ext2_fs.h>
#else
#include <linux/ext2_fs.h>
#endif
#include <ext2fs/ext2fs.h>],
[ext2_ino_t ino = 0;],
[AC_DEFINE([HAVE_EXT2_INO_T],1,[Define if we have the ext2_ino_t type (from e2fsprogs 1.20+).])
 AC_MSG_RESULT(yes)],
AC_MSG_RESULT(no))

dnl
dnl Check for s_journal_inum field in ext2_super_block struct
dnl
AC_MSG_CHECKING(for s_journal_inum field in ext2_super_block struct)
AC_TRY_COMPILE([#include <stdio.h>
#ifdef HAVE_EXT2FS_EXT2_FS_H
#include <ext2fs/ext2_fs.h>
#else
#include <linux/ext2_fs.h>
#endif
#include <ext2fs/ext2fs.h>],
[struct ext2_super_block es; es.s_journal_inum = 0;],
[AC_DEFINE([HAVE_EXT2_JOURNAL_INUM],1,[Define if we have the s_journal_inum field in struct ext2_super_block.])
 AC_MSG_RESULT(yes)],
AC_MSG_RESULT(no))

dnl
dnl Check for blkid headers libraries
dnl
AC_ARG_ENABLE([blkid], [AC_HELP_STRING([--disable-blkid], [enable blkid support])])
if test "$enable_blkid" != "no"; then
	PKG_CHECK_MODULES([BLKID], [blkid], [], [
		found_blkid="no"
		if test "$enable_blkid" = "yes"; then
			AC_MSG_ERROR([blkid support requested, but not found])
		fi
	])
	if test "$found_blkid" != "no"; then
		AC_DEFINE([HAVE_BLKID], 1, [Define this if you have the blkid library.])
	fi
fi

AC_ARG_ENABLE([uuid], [AC_HELP_STRING([--disable-uuid], [enable uuid support])])
if test "$enable_uuid" != "no"; then
	PKG_CHECK_MODULES([UUID], [uuid], [], [
		found_uuid="no"
		if test "$enable_uuid" = "yes"; then
			AC_MSG_ERROR([uuid support requested, but not found])
		fi
	])
	if test "$found_uuid" != "no"; then
		AC_DEFINE([HAVE_UUID], 1, [Define this if you have the uuid library.])
	fi
fi

dnl
dnl Check for ncurses or termcap libraries
dnl
AC_CHECK_LIB(ncurses, tgetent, [ncurses_lib=yes], [ncurses_lib=no])
AC_CHECK_LIB(termcap, tgetent, [termcap_lib=yes], [termcap_lib=no])
AC_CHECK_LIB(tinfo, tgetent, [tinfo_lib=yes], [tinfo_lib=no])

if test "$ncurses_lib" = no -a "$termcap_lib" = no -a "$tinfo_lib" = no; then
	if test "$READLINE" = "yes"; then
		AC_MSG_ERROR(You need to install the ncurses or termcap library or configure without --enable-readline)
	fi
fi
if test "$tinfo_lib" = yes; then
	rdllib="-ltinfo"
elif test "$ncurses_lib" = yes; then
	rdllib="-lncurses"
elif test "$termcap_lib" = yes; then
	rdllib="-ltermcap"
fi

dnl
dnl Check for readline headers and libraries
dnl
AC_CHECK_HEADER(readline/readline.h, [readline_h=yes], [readline_h=no], [-])
AC_CHECK_LIB(readline, readline, [readline_lib=yes], [readline_lib=no], $rdllib)
if test "$readline_h" = no -o "$readline_lib" = no; then
	if test "$READLINE" = "yes"; then
		AC_MSG_ERROR(You need to install the GNU readline library or configure without --enable-readline)
	fi
fi
if test "$READLINE" = yes; then
	READLINE="-lreadline $rdllib"
fi
AC_SUBST(READLINE)

dnl
dnl Check for rl_completion_matches
dnl
AC_CHECK_LIB(readline, rl_completion_matches, [rlcm=yes], [rlcm=no], [$rdllib])
if test "$rlcm" = yes; then
	AC_DEFINE([HAVE_READLINE_RLCM],1,[Define this if your readline libs have the rl_completion_matches library.])
fi

dnl
dnl Check for rl_completion_append_character
dnl
AC_CHECK_LIB(readline, rl_completion_append_character, [rcac=yes], [rcac=no], [$rdllib])
if test "$rcac" = yes; then
	AC_DEFINE([HAVE_READLINE_CAC],1,[Define this if your readline libs have the rl_completion_append_character variable.])
fi

dnl
dnl Check for zlib headers and libraries
dnl
AC_ARG_ENABLE([zlib], [AC_HELP_STRING([--disable-zlib], [enable zlib compression support])])
if test "$enable_zlib" != "no"; then
	PKG_CHECK_MODULES([ZLIB], [zlib], [], [
		found_zlib="no"
		if test "$enable_zlib" = "yes"; then
			AC_MSG_ERROR([zlib support requested, but not found])
		fi
	])
	if test "$found_zlib" != "no"; then
		AC_DEFINE([HAVE_ZLIB], 1, [Include zlib support])
	fi
fi

dnl
dnl Check for bzlib headers and libraries
dnl
AC_ARG_ENABLE([bzip2], [AC_HELP_STRING([--disable-bzip2], [enable bzip2 compression support])])
BZLIB=
if test "$enable_bzip2" != "no"; then
	AC_CHECK_HEADER(bzlib.h, [bzlib_h=yes], [bzlib_h=no], [-])
	AC_CHECK_LIB(bz2, BZ2_bzBuffToBuffCompress, [bzlib_lib=yes], [bzlib_lib=no])

	if test "$bzlib_h" = yes -a "$bzlib_lib" = yes; then
		BZLIB="-lbz2"
		AC_DEFINE([HAVE_BZLIB], 1, [Define this if you have bzlib compression library.])
	elif test "$enable_bzip2" = "yes"; then
		AC_MSG_ERROR([bzip2 support requested, but not found])
	fi
fi
AC_SUBST(BZLIB)

dnl
dnl Check for lzo headers and libraries
dnl
AC_ARG_ENABLE([lzo], [AC_HELP_STRING([--disable-lzo], [enable lzo compression support])])
LZOLIB=
if test "$enable_lzo" != "no"; then
	AC_CHECK_HEADER(lzo/lzo1x.h, [lzo_h=yes], [lzo_h=no], [-])
	AC_CHECK_LIB(lzo2, lzo1x_1_compress, [lzo_lib=yes], [lzo_lib=no])

	if test "$lzo_h" = yes -a "$lzo_lib" = yes; then
		LZOLIB="-llzo2"
		AC_DEFINE([HAVE_LZO], 1, [Define this if you have lzo compression library.])
	elif test "$enable_lzo" = "yes"; then
		AC_MSG_ERROR([lzo support requested, but not found])
	fi
fi
AC_SUBST(LZOLIB)

dnl
dnl Check for sqlite3 headers and libraries
dnl
AC_ARG_ENABLE([sqlite], [AC_HELP_STRING([--disable-sqlite], [disable sqlite indexing support])])
SQLITE3=
if test "$enable_sqlite" != "no"; then
	AC_CHECK_HEADER(sqlite3.h, [sqlite3_h=yes], [sqlite3_h=no], [-])
	AC_CHECK_LIB(sqlite3, sqlite3_initialize, [sqlite3_lib=yes], [sqlite3_lib=no])
	if test "$sqlite3_h" = yes -a "$sqlite3_lib" = yes; then
		SQLITE3="-lsqlite3"
	    AC_DEFINE([HAVE_SQLITE3],1,[Define this if you have sqlite3 library.])
	elif test "$enable_sqlite" = "yes"; then
		AC_MSG_ERROR([sqlite support requested, but not found])
    fi
fi
AC_SUBST(SQLITE3)

dnl
dnl Check for library functions
dnl
AC_CHECK_FUNCS(err errx verr verrx vwarn vwarnx warn warnx realpath lchown)
AC_CHECK_FUNC(glob)

dnl
dnl Check for GLOB_ALTDIRFUNC
dnl
AC_MSG_CHECKING(for extended glob routines)
if test "$ac_cv_func_glob" = "yes"; then
	AC_EGREP_CPP(yes,
	[
#	include <glob.h>
#	ifdef GLOB_ALTDIRFUNC
	yes
#	endif
	],
	[
	AC_DEFINE([HAVE_GLOB],1,[Define if you have the glob function.])
	AC_MSG_RESULT(yes)
	],
	[
	AC_MSG_RESULT(no)
	echo "Your system does not support extended glob, will use the internal routines"
	])
fi

dnl
dnl Check for OpenSSL, for ermt and encryption.
dnl
AC_ARG_ENABLE([ssl], [AC_HELP_STRING([--disable-ssl], [disable ssl support])])
SSLLIB=
if test "$enable_ssl" != "no"; then
	AC_CHECK_HEADER(openssl/evp.h, [evp_h=yes], [evp_h=no])
	AC_CHECK_LIB(crypto, EVP_CIPHER_CTX_set_padding, [crypto_lib=yes], [crypto_lib=no])
	if test "$evp_h" = yes -a "$crypto_lib" = yes; then
		SSLLIB="-lssl"
		AC_DEFINE([HAVE_OPENSSL],1,[Define this if you have openssl library.])
	elif test "$enable_ssl" = "yes"; then
		AC_MSG_ERROR([ssl support requested, but not found])
	fi
fi
if test "$ERMT" != "" -a -z "$SSLLIB"; then
	AC_MSG_ERROR([You need to install the OpenSSL library (version 0.9.7a or later), or configure without --enable-ermt])
fi
AC_SUBST(SSLLIB)

dnl
dnl Check for types
dnl
AC_TYPE_INT64_T
AC_TYPE_UINT64_T

dnl
dnl Output files
dnl
AC_CONFIG_FILES(m4_flatten([
	Makefile
	common/Makefile
	compat/lib/Makefile
	dump/Makefile
	restore/Makefile
	rmt/Makefile
]))
AC_OUTPUT
