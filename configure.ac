AC_INIT([dump], [0.4b44], [http://dump.sourceforge.net])
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

AC_DEFINE([HAVE_LZO], 1, [Enable LZO transformation support])

dnl
dnl Handle --enable-debug
dnl
AC_MSG_CHECKING([whether to enable extra debugging])
AC_ARG_ENABLE([debug],
[  --enable-debug             include debugging code (default is NO)])
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
AC_ARG_ENABLE([static-progs],
[  --enable-static-progs      link dump and restore statically (default is NO)])
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
AC_ARG_ENABLE([rmt],
[  --enable-rmt               compile and install rmt (default is YES)])
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
AC_ARG_ENABLE([ermt],
[  --enable-ermt              compile ermt, an encrypting version of rmt (default is NO)])
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
AC_ARG_ENABLE([kerberos],
[  --enable-kerberos          compile kerberos extensions (default is NO)])
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
AC_ARG_ENABLE([readline],
[  --enable-readline          enable readline support in restore (default is YES)],
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
AC_ARG_ENABLE([oldstylefscript],
[  --enable-oldstylefscript   enable old style F script (no arguments) (default is NO)],
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
AC_ARG_ENABLE([qfa],
[  --enable-qfa               enable Quick File Access support (default is YES)],
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
AC_ARG_ENABLE([qfadebug],
[  --enable-qfadebug          include Quick File Access debugging code (default is NO)],
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
AC_ARG_ENABLE([macosx],
[  --enable-macosx            include Mac OSX restore compatibility (default is NO)],
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
AC_ARG_ENABLE([selinux],
[  --enable-selinux      restore can translate SELinux EAs (default is auto)])
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
AC_ARG_WITH([dumpdatespath],
[  --with-dumpdatespath=PATH  select path for dumpdates file],
AC_MSG_RESULT(DUMPDATESPATH is $withval)
DUMPDATESPATH=$withval,
DUMPDATESPATH="${sysconfdir}/dumpdates"
echo "DUMPDATESPATH defaults to $DUMPDATESPATH"
)dnl
AC_SUBST(DUMPDATESPATH)

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
AC_CHECK_HEADER(blkid/blkid.h, [blkid_h=yes], [blkid_h=no], [-])
PKG_CHECK_EXISTS([blkid],
	[
	BLKID=`$PKG_CONFIG --libs blkid`
	if test "$blkid_h" = yes ; then
		AC_DEFINE([HAVE_BLKID],1,[Define this if you have the blkid library.])
	fi
	],[BLKID=""])
AC_SUBST(BLKID)

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
AC_CHECK_LIB(readline, rl_completion_matches, [rlcm=yes], [rlcm=no], "-ltermcap")
if test "$rlcm" = yes; then
	AC_DEFINE([HAVE_READLINE_RLCM],1,[Define this if your readline libs have the rl_completion_matches library.])
fi

dnl
dnl Check for rl_completion_append_character
dnl
AC_CHECK_LIB(readline, rl_completion_append_character, [rcac=yes], [rcac=no], "-ltermcap")
if test "$rcac" = yes; then
	AC_DEFINE([HAVE_READLINE_CAC],1,[Define this if your readline libs have the rl_completion_append_character variable.])
fi

dnl
dnl Check for zlib headers and libraries
dnl
AC_CHECK_HEADER(zlib.h, [zlib_h=yes], [zlib_h=no], [-])
AC_CHECK_LIB(z, zlibVersion, [zlib_lib=yes], [zlib_lib=no])
if test "$zlib_h" = yes -a "$zlib_lib" = yes; then
	ZLIB="-lz"
	AC_DEFINE([HAVE_ZLIB],1,[Define this if you have zlib compression library.])
else
	ZLIB=""
fi
AC_SUBST(ZLIB)

dnl
dnl Check for bzlib headers and libraries
dnl
AC_CHECK_HEADER(bzlib.h, [bzlib_h=yes], [bzlib_h=no], [-])
AC_CHECK_LIB(bz2, BZ2_bzBuffToBuffCompress, [bzlib_lib=yes], [bzlib_lib=no])
if test "$bzlib_h" = yes -a "$bzlib_lib" = yes; then
	BZLIB="-lbz2"
	AC_DEFINE([HAVE_BZLIB],1,[Define this if you have bzlib compression library.])
else
	BZLIB=""
fi
AC_SUBST(BZLIB)

dnl
dnl Check for sqlite3 headers and libraries
dnl
AC_CHECK_HEADER(sqlite3.h, [sqlite3_h=yes], [sqlite3_h=no], [-])
AC_CHECK_LIB(sqlite3, sqlite3_initialize, [sqlite3_lib=yes], [sqlite3_lib=no])
if test "$sqlite3_h" = yes -a "$sqlite3_lib" = yes; then
	SQLITE3="-lsqlite3"
    AC_DEFINE([HAVE_SQLITE3],1,[Define this if you have sqlite3 library.])
else
    SQLITE3=""
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
AC_CHECK_HEADER(openssl/evp.h, [evp_h=yes], [evp_h=no])
AC_CHECK_LIB(crypto, EVP_CIPHER_CTX_set_padding, [crypto_lib=yes], [crypto_lib=no])
if test "$evp_h" = yes -a "$crypto_lib" = yes; then
	SSLLIB="-lssl"
	AC_DEFINE([HAVE_OPENSSL],1,[Define this if you have openssl library.])
else
	SSLLIB=""
	if test "$ERMT" != ""; then
		AC_MSG_ERROR(You need to install the OpenSSL library (version 0.9.7a or later), or configure without --enable-ermt)
	fi
fi
AC_SUBST(SSLLIB)

dnl
dnl Check for types
dnl
AC_CHECK_TYPE(quad_t, int64_t)
AC_CHECK_TYPE(u_quad_t, uint64_t)

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
