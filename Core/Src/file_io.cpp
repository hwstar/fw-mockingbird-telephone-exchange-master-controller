#include "top.h"
#include "file_io.h"
#include "logging.h"

namespace File_Io {

const char *TAG = "fileio";

const char *file_error_strings[MAX_ERROR_MESSAGES] = {
		"no error",
		"bad file descriptor",
		"no such file or directory",
		"i/o error",
		"file name too long",
		"too many open files",
		"file exists",
		"no space left on device",
		"not a directory",
		"directory not empty",
		"permissions error",
		"invalid seek",
		"invalid argument"

};


int32_t File_Io::_map_error_code(int32_t fd, FRESULT fres) {
	int32_t res;

	/* Save error code */

	if(fd >= 0 || (fd < MAX_OPEN_FILES)) {
		this->_last_fatfs_error_code[fd] = fres;
	}

	/* Map error codes */

		switch(fres) {
		case FR_OK:
			res = EINFO_NOERR;
			break;

		case FR_DISK_ERR:
		case FR_INT_ERR:
		case FR_NOT_READY:
		case FR_INVALID_OBJECT:
		case FR_INVALID_DRIVE:
		case FR_NOT_ENABLED:
		case FR_NO_FILESYSTEM:
		case FR_TIMEOUT:
		case FR_LOCKED:
		case FR_NOT_ENOUGH_CORE:
			res = EINFO_EIO;
			break;

		case FR_NO_FILE:
		case FR_NO_PATH:
		case FR_INVALID_NAME:
			res = EINFO_ENOENT;
			break;

		case FR_DENIED:
		case FR_WRITE_PROTECTED:
			res = EINFO_EPERM;
			break;

		case FR_EXIST:
			res = EINFO_EEXIST;
			break;

		case FR_TOO_MANY_OPEN_FILES:
			res = EINFO_ENFILE;
			break;

		default:
			res = EINFO_EIO;
			break;
		}
	return res;


}

/*
 * Validate file descriptor
 */

bool File_Io::_validate_file_descriptor(int32_t fd) {

	/* fd must be within range */
	if((fd < 0) || fd >= MAX_OPEN_FILES) {
		return false;
	}

	/* fd must be open */
	if((this->_fd_in_use_bits & (1 << fd)) == 0) {
		return false;
	}

	return true;
}


/*
 * Initialize file system
 */

void File_Io::init(void) {
	FRESULT fres;

	/* Mutex attributes */
	static const osMutexAttr_t fileio_mutex_attr = {
		"FileIOMutex",
		osMutexRecursive | osMutexPrioInherit,
		NULL,
		0U
	};

	/* Create mutex to protect file i/o data between tasks */


	this->_lock = osMutexNew(&fileio_mutex_attr);
	if (this->_lock == NULL) {
			LOG_PANIC(TAG, "Could not create lock");
		  }

	/* Physically mount the volume */

	fres = f_mount(&this->_fs, "", 1);
	if(fres != FR_OK) {
		LOG_PANIC(TAG, "SD Card not present, insert SD Card and reboot");
	}

}

/*
 * Open a file
 */

int32_t File_Io::open(const char *name, uint32_t mode) {
	uint8_t fd_index;
	int32_t res = EINFO_NOERR;
	FRESULT fr;

	/* Only support open for read for now */
	if(mode != O_RDONLY) {
		return EINFO_EPERM; /* Permissions error */
	}

	/* Get the lock */
	osMutexAcquire(this->_lock, osWaitForever);

	/* Set the appropriate in use bit */
	for(fd_index = 0; fd_index < MAX_OPEN_FILES; fd_index++) {
		if((this->_fd_in_use_bits & (1 << fd_index)) == 0) {
			this->_fd_in_use_bits |= (1 << fd_index);
			break;
		}
	}

	/* Error if no available file descriptor */
	if(fd_index >= MAX_OPEN_FILES) {
		/* Release the lock */
		osMutexRelease(this->_lock);
		return EINFO_ENFILE; /* Too many open files */
	}

	UINT f_opts = FA_READ;

	/* Call the underlying FATFS api */
	fr = f_open(&this->_fo[fd_index], name, f_opts);
	res = this->_map_error_code(fd_index, fr);

	/* Release the lock */
	osMutexRelease(this->_lock);
	/* Return file descriptor if no error */
	if(res == EINFO_NOERR) {
		res = fd_index;
	}
	return res;
}

/*
* Close a file
*/

int32_t File_Io::close(int32_t fd) {
	int32_t res = EINFO_NOERR;
	FRESULT fr;

	if(!this->_validate_file_descriptor(fd)) {
		return EINFO_EBADF;
	}

	/* Get the lock */
	osMutexAcquire(this->_lock, osWaitForever);

	/* Clear the appropriate in use bit */

	this->_fd_in_use_bits &= ~(1 << fd);


	/* Call underlying FATFS api */
	fr = f_close(&this->_fo[fd]);

	/* Map error code */
	res = this->_map_error_code(fd, fr);

	/* Release the lock */
	osMutexRelease(this->_lock);

	return res;

}

/*
 * Read from file
 */

int32_t File_Io::read(int32_t fd, uint8_t *buffer, int32_t count) {
	UINT br;
	FRESULT fr;
	int32_t res;

	/* Check arguments */

	if((count < 0) || (!buffer)) {
		return EINFO_EINVAL;
	}

	if(!this->_validate_file_descriptor(fd)) {
		return EINFO_EBADF;
	}

	/* Call underlying FATFS api */
	fr = f_read(&this->_fo[fd], buffer, count, &br);

	/* Map error code */
	res = this->_map_error_code(fd, fr);

	if(res == EINFO_NOERR) {
		/* Return actual number of bytes read */
		res = (int32_t) br;
	}

	return res;

}

/*
 * Return the size of an open file
 */


int32_t File_Io::fsize(int32_t fd) {

	/* Validate file descriptor */

	if(!this->_validate_file_descriptor(fd)) {
		return EINFO_EBADF;
	}

	return (int32_t) f_size(&this->_fo[fd]);

}




/*
 * Write to file
 */

int32_t File_Io::write(int32_t fd, uint8_t *buffer, int32_t count) {


	return -1;

}

/*
 * Return error string from error number passed in
 */

const char *File_Io::error_string(int32_t error_number) {
	if((error_number < 0) || (error_number > EINFO_END_MARKER) ) {
		return file_error_strings[-error_number];
	}
	else if(error_number == 0) {
		return file_error_strings[0];
	}
	else {
		return "unknown error";
	}
}


} /* End namespace file_io */

File_Io::File_Io File_io;
