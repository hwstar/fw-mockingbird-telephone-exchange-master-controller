#pragma once
#include "fatfs.h"

namespace File_Io {

const uint8_t MAX_OPEN_FILES = 8;




enum {EINFO_NOERR = 0, EINFO_EBADF = -1, EINFO_ENOENT=-2, EINFO_EIO=-3,
	EINFO_ENAMETOOLONG=-4, EINFO_ENFILE=-5,  EINFO_EEXIST=-6, EINFO_ENOSPC=-7,
	EINFO_ENOTDIR=-8, EINFO_ENOTEMPTY=-9, EINFO_EPERM = -10, EINFO_PIPE = -11, EINFO_EINVAL=-12,


	EINFO_END_MARKER = -13
	};
const char MAX_ERROR_MESSAGES = (EINFO_NOERR - EINFO_END_MARKER);

enum {O_RDONLY=1};

class File_Io {
public:
	void init(void);
	int32_t open(const char *name, uint32_t mode);
	int32_t close(int32_t fd);
	int32_t read(int32_t fd, uint8_t *buffer, int32_t count);
	int32_t write(int32_t fd, uint8_t *buffer, int32_t count);
	int32_t fsize(int32_t fd);
	const char *error_string(int32_t error_number);

protected:
	int32_t _map_error_code(int32_t fd, FRESULT fres);
	bool _validate_file_descriptor(int32_t fd);

	osMutexId_t _lock;
	FATFS _fs;
	FIL _fo[MAX_OPEN_FILES];
	FRESULT _last_fatfs_error_code[MAX_OPEN_FILES];
	uint8_t _fd_in_use_bits;


};


} /* End namespace file_io */

extern File_Io::File_Io File_io;
