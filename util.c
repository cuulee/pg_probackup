/*-------------------------------------------------------------------------
 *
 * util.c: log messages to log file or stderr, and misc code.
 *
 * Copyright (c) 2009-2011, NIPPON TELEGRAPH AND TELEPHONE CORPORATION
 *
 *-------------------------------------------------------------------------
 */

#include "pg_arman.h"

#include <time.h>

static void
checkControlFile(ControlFileData *ControlFile)
{
	pg_crc32c   crc;

	/* Calculate CRC */
	INIT_CRC32C(crc);
	COMP_CRC32C(crc, (char *) ControlFile, offsetof(ControlFileData, crc));
	FIN_CRC32C(crc);

	/* Then compare it */
    if (!EQ_CRC32C(crc, ControlFile->crc))
		elog(ERROR_CORRUPTED, "Calculated CRC checksum does not match value stored in file.\n"
			 "Either the file is corrupt, or it has a different layout than this program\n"
			 "is expecting. The results below are untrustworthy.\n");

	if (ControlFile->pg_control_version % 65536 == 0 && ControlFile->pg_control_version / 65536 != 0)
		elog(ERROR_CORRUPTED, "possible byte ordering mismatch\n"
			 "The byte ordering used to store the pg_control file might not match the one\n"
			 "used by this program. In that case the results below would be incorrect, and\n"
			 "the PostgreSQL installation would be incompatible with this data directory.\n");
}

/*
 * Verify control file contents in the buffer src, and copy it to *ControlFile.
 */
void
digestControlFile(ControlFileData *ControlFile, char *src, size_t size)
{
	if (size != PG_CONTROL_SIZE)
		elog(ERROR_PG_INCOMPATIBLE, "unexpected control file size %d, expected %d\n",
			 (int) size, PG_CONTROL_SIZE);

	memcpy(ControlFile, src, sizeof(ControlFileData));

	/* Additional checks on control file */
	checkControlFile(ControlFile);
}

/*
 * Utility shared by backup and restore to fetch the current timeline
 * used by a node.
 */
TimeLineID
get_current_timeline(void)
{
	ControlFileData ControlFile;
	char       *buffer;
	size_t      size;

	/* First fetch file... */
	buffer = slurpFile(pgdata, "global/pg_control", &size);
	digestControlFile(&ControlFile, buffer, size);
	pg_free(buffer);

	return ControlFile.checkPointCopy.ThisTimeLineID;
}

/*
 * Convert time_t value to ISO-8601 format string
 */
void
time2iso(char *buf, size_t len, time_t time)
{
	struct tm *tm = localtime(&time);

	strftime(buf, len, "%Y-%m-%d %H:%M:%S", tm);
}

const char *
status2str(BackupStatus status)
{
	static const char *statusName[] =
	{
		"UNKNOWN",
		"OK",
		"RUNNING",
		"ERROR",
		"DELETING",
		"DELETED",
		"DONE",
		"CORRUPT"
	};

	if (status < BACKUP_STATUS_INVALID || BACKUP_STATUS_CORRUPT < status)
		return "UNKNOWN";

	return statusName[status];
}

void
remove_trailing_space(char *buf, int comment_mark)
{
	int		i;
	char   *last_char = NULL;

	for (i = 0; buf[i]; i++)
	{
		if (buf[i] == comment_mark || buf[i] == '\n' || buf[i] == '\r')
		{
			buf[i] = '\0';
			break;
		}
	}
	for (i = 0; buf[i]; i++)
	{
		if (!isspace(buf[i]))
			last_char = buf + i;
	}
	if (last_char != NULL)
		*(last_char + 1) = '\0';

}

void
remove_not_digit(char *buf, size_t len, const char *str)
{
	int i, j;

	for (i = 0, j = 0; str[i] && j < len; i++)
	{
		if (!isdigit(str[i]))
			continue;
		buf[j++] = str[i];
	}
	buf[j] = '\0';
}
