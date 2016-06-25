#include <config.h>
#include <ctype.h>
#include <compaterr.h>
#include <fcntl.h>
#include <fstab.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <sys/param.h>
#include <sys/time.h>
#include <time.h>
#ifdef __linux__
#include <linux/types.h>
#include <sys/stat.h>
#include <sys/mtio.h>
#include <bsdcompat.h>
#include <linux/fs.h>	/* for definition of BLKFLSBUF */
#elif defined sunos
#include <sys/vnode.h>

#include <ufs/inode.h>
#include <ufs/fs.h>
#else
#include <ufs/ufs/dinode.h>
#include <ufs/ffs/fs.h>
#endif

#include <protocols/dumprestore.h>

//#include "dump.h"
#include "indexer.h"
#include "slave.h"

extern dump_ino_t volinfo[];  // TP_NINOS
extern int tapeno;

static int Afile = -1;	/* archive file descriptor */
static int AfileActive = 1;/* Afile flag */

//extern char *Apath;
char *Apath;

#ifdef USE_QFA
static int GetTapePos (long long *);
static int MkTapeString (struct s_spcl *, long long);
#define FILESQFAPOS	20

int tapepos;
int ntrec;		/* blocking factor on tape */
int magtapeout;  /* is output a magnetic tape? */
//extern int tapepos;
//extern int ntrec;		/* blocking factor on tape */
//extern int magtapeout;  /* is output a magnetic tape? */

static int gtperr = 0;
static int gTapeposfd = -1;			/* code below assumes fd >= 0 means do print */
const char *gTapeposfile;
static char gTps[255];
static int32_t gThisDumpDate;
#endif /* USE_QFA */

void	msg (const char *fmt, ...);
void	quit (const char *fmt, ...);

extern off_t rmtseek(off_t, int);
extern void mkchecksum(union u_spcl *tmpspcl);

extern char *host;
extern int tapefd;

/*
 * Open the indexer file.
 */
static int
legacy_open(const char *filename, int mode)
{
	if (filename == NULL) {
		return 0;
	}

	if (mode == 0) {
		if ((Afile = open(filename, O_RDONLY)) < 0) {
			msg("Cannot open %s for reading: %s\n",
					filename, strerror(errno));
			msg("The ENTIRE dump is aborted.\n");
			return -1;
		}
	} else if (mode == 1) {
		if ((Afile = open(filename, O_WRONLY|O_CREAT|O_TRUNC,
				   S_IRUSR | S_IWUSR | S_IRGRP |
				   S_IWGRP | S_IROTH | S_IWOTH)) < 0) {
			msg("Cannot open %s for writing: %s\n",
					filename, strerror(errno));
			msg("The ENTIRE dump is aborted.\n");
			return -1;
		}
	}

	return 0;
}

/*
 * Close the indexer file.
 */
static int
legacy_close()
{
	if (Afile >= 0)
		msg("Archiving dump to %s\n", Apath);
	return 0;
}

/*
 * Write a record to the indexer file.
 */
static int
legacy_writerec(const void *dp, int isspcl)
{
	if (! AfileActive && isspcl && (spcl.c_type == TS_END))
		AfileActive = 1;
	if (AfileActive && Afile >= 0 && !(spcl.c_flags & DR_EXTATTRIBUTES)) {
		/* When we dump an inode which is not a directory,
		 * it means we ended the archive contents */
		if (isspcl && (spcl.c_type == TS_INODE) &&
		    ((spcl.c_dinode.di_mode & S_IFMT) != IFDIR))
			AfileActive = 0;
		else {
			union u_spcl tmp;
			tmp = *(union u_spcl *)dp;
			/* Write the record, _uncompressed_ */
			if (isspcl) {
				tmp.s_spcl.c_flags &= ~DR_COMPRESSED;
				mkchecksum(&tmp);
			}
			if (write(Afile, &tmp, TP_BSIZE) != TP_BSIZE)
				msg("error writing archive file: %s\n",
			    	strerror(errno));
		}
	}
	return 0;
}

/*
 * I'm not sure what this is used for...
 */
static int
legacy_foo()
{
	if (Afile >= 0) {
		volinfo[1] = ROOTINO;
		memcpy(spcl.c_inos, volinfo, TP_NINOS * sizeof(dump_ino_t));
		spcl.c_flags |= DR_INODEINFO;
	}
	return 0;
}

/*
 * Dump inode
 */
static int
legacy_addInode(struct dinode *dp, dump_ino_t ino, int metaonly)
{
	return 0;
}

/*
 * Dump directory (dirent) entry
 */
static int
legacy_addDirEntry(struct direct *dp, dump_ino_t parent_ino)
{
	 return 0;
}


#define LSEEK_GET_TAPEPOS 	10
#define LSEEK_GO2_TAPEPOS	11

/*
 *
 */
static int
legacy_openQfa()
{
#ifdef USE_QFA
	gThisDumpDate = spcl.c_date;
	if (tapepos) {
		msg("writing QFA positions to %s\n", gTapeposfile);
		if ((gTapeposfd = open(gTapeposfile,
				       O_WRONLY|O_CREAT|O_TRUNC,
				       S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP
				       | S_IROTH | S_IWOTH)) < 0)
			quit("can't open tapeposfile\n");
		/* print QFA-file header */
		snprintf(gTps, sizeof(gTps), "%s\n%s\n%ld\n\n", QFA_MAGIC, QFA_VERSION, (unsigned long)spcl.c_date);
		gTps[sizeof(gTps) - 1] = '\0';
		if (write(gTapeposfd, gTps, strlen(gTps)) != (ssize_t)strlen(gTps))
			quit("can't write tapeposfile\n");
		sprintf(gTps, "ino\ttapeno\ttapepos\n");
		if (write(gTapeposfd, gTps, strlen(gTps)) != (ssize_t)strlen(gTps))
			quit("can't write tapeposfile\n");
	}
#endif /* USE_QFA */
	return 0;
}

/*
 *
 */
static int
legacy_closeQfa()
{
	return 0;
}

/*
 *
 */
static int
legacy_openQfaState(QFA_State *s)
{
	s->curtapepos = 0;
	s->maxntrecs = 50000;	/* every 50MB */
	s->cntntrecs = s->maxntrecs;
	return 0;
}

/*
 *
 */
static int
legacy_updateQfaState(QFA_State *s)
{
#ifdef USE_QFA
	if (gTapeposfd >= 0) {
		s->cntntrecs += ntrec;
	}
#endif

	return 0;
}


/*
 *
 */
static int
legacy_updateQfa(QFA_State *s)
{
#ifdef USE_QFA
	union u_spcl *uspclptr;
	struct s_spcl *spclptr;

	if (gTapeposfd >= 0) {
		int i;
		int foundone = 0;

		for (i = 0; (i < ntrec) && !foundone; ++i) {
			uspclptr = (union u_spcl *)&slp->tblock[i];
			spclptr = &uspclptr->s_spcl;
			if ((spclptr->c_magic == NFS_MAGIC) &&
						(spclptr->c_type == TS_INODE) &&
						(spclptr->c_date == gThisDumpDate) &&
						!(spclptr->c_dinode.di_mode & S_IFDIR) &&
						!(spclptr->c_flags & DR_EXTATTRIBUTES)
					) {
				foundone = 1;
				/* if (s->cntntrecs >= s->maxntrecs) {	 only write every maxntrecs amount of data */
					s->cntntrecs = 0;
					if (gtperr == 0)
						gtperr = GetTapePos(&s->curtapepos);
					/* if an error occured previously don't
					 * try again */
					if (gtperr == 0) {
#ifdef DEBUG_QFA
						msg("inode %ld at tapepos %ld\n", spclptr->c_inumber, s->curtapepos);
#endif
						gtperr = MkTapeString(spclptr, s->curtapepos);
					}
				/* } */
			}
		}
	}
#endif /* USE_QFA */
	return 0;
}

#ifdef USE_QFA
/*
 * read the current tape position
 */
static int
GetTapePos(long long *pos)
{
	int err = 0;

#ifdef RDUMP
	if (host) {
		*pos = (long long) rmtseek((off_t)0, (int)LSEEK_GET_TAPEPOS);
		err = *pos < 0;
	}
	else
#endif
	{
	if (magtapeout) {
		long mtpos;
		*pos = 0;
		err = (ioctl(tapefd, MTIOCPOS, &mtpos) < 0);
		*pos = (long long)mtpos;
	}
	else {
		*pos = lseek(tapefd, 0, SEEK_CUR);
		err = (*pos < 0);
	}
	}
	if (err) {
		err = errno;
		msg("[%ld] error: %d (getting tapepos: %lld)\n", getpid(),
			err, *pos);
		return err;
	}
	return err;
}

static int
MkTapeString(struct s_spcl *spclptr, long long curtapepos)
{
	int	err = 0;

#ifdef DEBUG_QFA
	msg("inode %ld at tapepos %lld\n", spclptr->c_inumber, curtapepos);
#endif

	snprintf(gTps, sizeof(gTps), "%ld\t%d\t%lld\n",
		 (unsigned long)spclptr->c_inumber,
		 tapeno,
		 curtapepos);
	gTps[sizeof(gTps) - 1] = '\0';
	if (write(gTapeposfd, gTps, strlen(gTps)) != (ssize_t)strlen(gTps)) {
		err = errno;
		warn("error writing tapepos file. (error %d)\n", errno);
	}
	return err;
}
#endif /* USE_QFA */

Indexer indexer_legacy = {
	NULL,
	&legacy_open,
	&legacy_close,
	&legacy_foo,
	&legacy_writerec,
	&legacy_addInode,
	&legacy_addDirEntry,
	&legacy_openQfa,
	&legacy_closeQfa,
	&legacy_openQfaState,
	&legacy_updateQfa,
	&legacy_updateQfaState
};
