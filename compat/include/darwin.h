/*
 *
 */

#if defined(DUMP_MACOSX)

#ifndef DARWIN_H
#define DARWIN_H

#define INFOLEN 32
#define CORRECT 2


#define DARWIN_RSRC_NAME "/..namedfork/rsrc"
#define DARWIN_FINFO_NAME "/..namedfork/finfo"
/*
 * some structs from the HFS+ info for OS X
 * - including the header file causes messy clashes
 */

struct FndrFileInfo {
    u_int32_t     fdType;        /* file type */
    u_int32_t     fdCreator;    /* file creator */
    u_int16_t     fdFlags;    /* Finder flags */
    struct {
        int16_t    v;        /* file's location */
        int16_t    h;
    } fdLocation;
    int16_t     opaque;
};
typedef struct FndrFileInfo FndrFileInfo;

struct FndrDirInfo {
        struct {                        /* folder's window rectangle */
            int16_t     top;
            int16_t     left;
            int16_t     bottom;
            int16_t     right;
        } frRect;
        unsigned short  frFlags;        /* Finder flags */
        struct {
            u_int16_t   v;              /* folder's location */
            u_int16_t   h;
        } frLocation;
        int16_t         opaque;
};
typedef struct FndrDirInfo FndrDirInfo;


struct FndrOpaqueInfo {
    int8_t opaque[16];
};
typedef struct FndrOpaqueInfo FndrOpaqueInfo;

struct fndrinfo_block_t {
        FndrFileInfo    finderInfo;
        FndrOpaqueInfo  extendedFinderInfo;
};
typedef struct fndrinfo_block_t fndrinfo_block_t;

struct attrinfo_block_t {
        unsigned long info_length;
	u_int32_t	objid_low;
	u_int32_t	objid_high;
	struct timespec created;
	struct timespec backup;
	union {
		fndrinfo_block_t	finfo;
		FndrDirInfo		dinfo;
        } o;
	off_t	rsrc_length;
};
typedef struct attrinfo_block_t attrinfo_block_t;


#define ASINGLE_MAGIC	0x00051600	/* all in one file */
#define ADOUBLE_MAGIC	0x00051607	/* resource + info, data separated */

#define ASD_VERSION1	0x00010000	/* the original version */
#define ASD_VERSION2	0x00020000	/* the second version */

typedef struct ASDHeader {
	u_long	magic;			/* either single or double */
	u_long	version;		/* ASD_VERSION2 */
	u_char	filler[16];		/* reserved, zero */
	u_short	entries;		/* the number of entries */
	/* the entries follow */
} ASDHeader, *ASDHeaderPtr, **ASDHeaderHandle;

typedef enum {
	EntryDataFork = 1,
	EntryRSRCFork,
	EntryRealName,
	EntryComment,
	EntryBWIcon,
	EntryColorIcon,
	EntryOldFileInfo,
	EntryFileDates,
	EntryFinderInfo,
	EntryMacFileInfo,
	EntryProDOSInfo,
	EntryMSDOSInfo,
	EntryShortName,
	EntryAFPFileInfo,
	EntryDirID
} ASDEntryType;

typedef struct {
	u_long	entryID;		/* the entry type (forced to long) */
	u_long	offset;			/* offset in file of entry */
	u_long	len;			/* length of entry */
} ASDEntry, *ASDEntryPtr, **ASDEntryHandle;

typedef struct {
	u_long	creationDate;
	u_long	modificationDate;
	u_long	backupDate;
	u_long	accessDate;
} FileDates, *FileDatesPtr;

typedef struct dumpfinderinfo {
 FndrFileInfo fndrinfo;	/* 0: size = 16 bytes, same for FndrDirInfo */
 u_int32_t createDate;	/* 16: date and time of creation */
 u_int32_t contentModDate;/* 20: date/time of last content modification */
 u_int32_t attributeModDate;/* 24: date/time of last attribute modification */
 u_int32_t accessDate; /* 30: date/time of last access (MacOS X only) */
 u_int32_t backupDate; /* 34: date/time of last backup */
 u_int32_t textEncoding;	/* 36: hint for name conversions */
 char filler[980]; /* 40: for later expansion, 40 + 980 - 4 = TP_BSIZE - 4 */
} DumpFinderInfo, *DumpFinderInfoPtr;

#endif /* DARWIN_H */
#endif
