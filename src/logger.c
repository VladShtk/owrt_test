#include <syslog.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <stdarg.h>
#include <string.h>
#include <stdbool.h>
#include <bzlib.h>
#include <sys/stat.h>
#include "logger.h"

#define LOG_LEVEL_ERROR    0
#define LOG_LEVEL_WARNING  1
#define LOG_LEVEL_MESSAGE  2
#define LOG_LEVEL_DEBUG    3
#define LOG_FILE_MAX_SIZE  256 * 1024
#define LOG_STRING_MAX_LEN 1000
#define LOG_STRING_MAX_NUM 1000
#define LOG_FILE_NAME_MAX_LEN 80

/*
 * Prefixes for the different logging levels
 */
#define LOG_PREFIX_MESSAGE ""
#define LOG_PREFIX_ERROR   "ERROR"
#define LOG_PREFIX_WARNING "WARNING"
#define LOG_PREFIX_DEBUG   "DEBUG"

/*
 * Logger internal sctructure
 */
struct logger_t {
	int max_log_level;
	int use_stdout;
	FILE* out_file;
	void (*logger_func) (const int level, const char*);
};

/*
 * Global var
*/
char _logFileFullName[LOG_FILE_NAME_MAX_LEN +1] = {'\0'};
int  _stringCounter = 0;

/*
 * Global log struct
*/  
static struct logger_t log_global_set;

static const char* LOG_LEVELS[] = { 
					LOG_PREFIX_ERROR,
				    LOG_PREFIX_WARNING,
				    LOG_PREFIX_MESSAGE,
				    LOG_PREFIX_DEBUG };

/*
 * Defines
*/
void print_to_syslog(const int level, const char* message);
void print_to_file(const int level, const char* message);

/*
 * Get size of file
 */
long int filesize( FILE *fp )
 {
    long int save_pos, size_of_file;

    save_pos = ftell( fp );
    fseek( fp, 0L, SEEK_END );
    size_of_file = ftell( fp );
    fseek( fp, save_pos, SEEK_SET );
    return( size_of_file );
 }

/*
 * Close remaining file descriptor and reset global params
 */
void cleanup_internal()
{
	if (log_global_set.out_file) {
		if (!log_global_set.use_stdout) {
			fclose(log_global_set.out_file);
		}

		log_global_set.use_stdout = 0;
		log_global_set.out_file = NULL;
	}
}

/*
 * Reset internal state and set syslog as default target
 */ 
void logger_reset_state(void)
{
	log_global_set.max_log_level = LOG_MAX_LEVEL_ERROR_WARNING_MESSAGE;
	cleanup_internal();
	log_global_set.logger_func = print_to_syslog;
}

/*
 * Print to syslog
 */
void print_to_syslog(const int level, const char* message)
{
	// Print to console
	if(level == LOG_LEVEL_MESSAGE)
		printf("%s\n", message);
	else
		fprintf(stdout, "%s: %s\n", LOG_LEVELS[level], message);

	// Print to syslog
	syslog(LOG_INFO, "[%s] %s\n", LOG_LEVELS[level], message);
}

/*
 * Print to file which can be a regular text file or STDOUT "file"
 * @param level - log level, message - message text
 */
void print_to_file(const int level, const char* message)
{
	int    res;
	struct tm* current_tm;
	time_t time_now;
	char   timeBuffer[30];

	time(&time_now);
	current_tm = localtime(&time_now);
	strftime(timeBuffer, 26, "%d.%m.%y %H:%M:%S", current_tm);

	// Print to console
	if(level == LOG_LEVEL_MESSAGE)
	{
		// Print to console
		printf("%s\n", message);

		// Print to file
		res = fprintf(log_global_set.out_file, "%s %s\n", timeBuffer, message);
	}
	else if(level == LOG_LEVEL_DEBUG)
	{
		// Print to console
		fprintf(stdout, "%s: %s\n", LOG_LEVELS[level], message);

		// Print to file
		res = fprintf(log_global_set.out_file, "%s [%s] %s\n", timeBuffer, LOG_LEVELS[level], message);
	}
	else
	{
		// Print to console
		fprintf(stderr, "%s: %s\n", LOG_LEVELS[level], message);

		// Print to file
		res = fprintf(log_global_set.out_file, "%s [%s] %s\n", timeBuffer, LOG_LEVELS[level], message);
	}

	if (res == -1) {
		print_to_syslog(LOG_LEVEL_ERROR, "Unable to write to log file! Last message:");
		print_to_syslog(LOG_LEVEL_ERROR, message);
		return;
	}

	fflush(log_global_set.out_file);
}

/*
 * Check
 */

/*
 * Archive log file if needed
 */

bool logger_archive()
{
	int len;
	int bzerror =  BZ_OK;
	int nBuf    =  LOG_STRING_MAX_LEN-1;
	char stringBuf[LOG_STRING_MAX_LEN];

	// Path buf
	char *pLogFileName;
	char logFilePath       [LOG_FILE_NAME_MAX_LEN + 1];
	char logZipFilePath    [LOG_FILE_NAME_MAX_LEN + 1];
	char logZipFileFullName[LOG_FILE_NAME_MAX_LEN * 2];

	// Time
	struct tm* current_tm;
	time_t time_now;
	char timeBuffer[30];

	// In/Out files
	FILE* fLogZip;
	FILE* fLog;

	// Check log file using
	if((log_global_set.use_stdout > 0) || log_global_set.out_file == NULL)
		return true;

	// Check log file size
	if(filesize(log_global_set.out_file) < LOG_FILE_MAX_SIZE)
		return true;

	// Check log file name
	if(strlen(_logFileFullName) == 0)
	{
		syslog(LOG_ERR, "\nFun: logger_archive(), Error: No log file name\n");
		fprintf(stdout, "\nFun: logger_archive(), Error: No log file name\n");
		return false;
	}

	// Get log file short name
	pLogFileName = strrchr(_logFileFullName, '/');
	if(pLogFileName == NULL)
	{
		syslog(LOG_ERR, "\nFun: logger_archive(), Error: Wrong file name %s", _logFileFullName);
		fprintf(stdout, "\nFun: logger_archive(), Error: Wrong file name %s", _logFileFullName);
		return false;
	}
	pLogFileName++;

	// Get log file path
	memset(logFilePath, '\0', LOG_FILE_NAME_MAX_LEN + 1);
	memcpy(logFilePath, _logFileFullName, strlen(_logFileFullName) - strlen(pLogFileName));

	// Get log zip file path
	memset(logZipFilePath, '\0', LOG_FILE_NAME_MAX_LEN+1);
	sprintf(logZipFilePath, "%s%s", logFilePath, "LogArchive/");

	struct stat st = {0};
	if (stat(logZipFilePath, &st) == -1)
	{
		if(mkdir(logZipFilePath, 0777) != 0)
		{
			syslog(LOG_ERR, "\nFun: logger_archive(), Error: No permission, folder %s", logZipFilePath);
			fprintf(stdout, "\nFun: logger_archive(), Error: No permission, folder %s", logZipFilePath);
			return false;
		}
	}

	// Get log zip full file path
	time(&time_now);
	current_tm = localtime(&time_now);
	strftime(timeBuffer, 30, "%d.%m.%y-%H_%M_%S", current_tm);

	sprintf(logZipFileFullName, "%s%s_%s.bz2", logZipFilePath, pLogFileName, timeBuffer);

	// Open files
	fLogZip = fopen (logZipFileFullName, "w" );
	if (!fLogZip)
	{
		syslog(LOG_ERR, "\nFun: logger_archive(), Error: Can't open file %s", logZipFileFullName);
		fprintf(stdout, "\nFun: logger_archive(), Error: Can't open file %s", logZipFileFullName);
		return false;
	}

	BZFILE *bfp = BZ2_bzWriteOpen(&bzerror, fLogZip, 9, 0, 30);

	if (bzerror != BZ_OK)
	{
	    BZ2_bzWriteClose(&bzerror, bfp, 0, NULL, NULL);
		syslog(LOG_ERR, "\nFun: logger_archive(), Error: BZ2_bzWriteOpen Can't open file %s", logZipFileFullName);
		fprintf(stdout, "\nFun: logger_archive(), Error: BZ2_bzWriteOpen Can't open file %s", logZipFileFullName);

	    fclose(fLogZip);
	    return false;
	}

	fLog = fopen (_logFileFullName, "r" );
	if(fLog == NULL)
	{
		syslog(LOG_ERR, "\nFun: logger_archive(), Error: Can't open file: %s", _logFileFullName);
		fprintf(stdout, "\nFun: logger_archive(), Error: Can't open file: %s", _logFileFullName);

		return false;
	}

	// Start archiving
	fprintf(stdout,  "Archiving log, please wait... file: %s", _logFileFullName);

	memset(stringBuf, 0, nBuf);
	while (fgets(stringBuf, nBuf, fLog) != NULL)
	{
	    len = strlen(stringBuf);
	    BZ2_bzWrite(&bzerror, bfp, stringBuf, len);
	    if (bzerror == BZ_IO_ERROR)
	    {
			syslog(LOG_ERR, "\nFun: logger_archive(), Error: BZ2_bzWrite error");
			fprintf(stdout, "\nFun: logger_archive(), Error: BZ2_bzWrite error");
	        return false;
	    }
	    memset(stringBuf, 0, nBuf);
	}
	BZ2_bzWriteClose(&bzerror, bfp, 0, NULL, NULL);

	// Close all files
    fclose(log_global_set.out_file);
    fclose(fLogZip);
    fclose(fLog);

    // Erase and open log file
    log_global_set.out_file = fopen( _logFileFullName, "w" );
	if (!log_global_set.out_file)
	{
		syslog(LOG_ERR, "\nFun: logger_archive(), Error: Can't open file %s for write\n", _logFileFullName);
		fprintf(stdout, "\nFun: logger_archive(), Error: Can't open file %s for write\n", _logFileFullName);
		return false;
	}

	fprintf(stdout,  "\nArchiving log done\n");

	return true;
}

/*
 * Init loger
 * @param filename - lof file name
 */
bool logger_init(const char* filename)
{
	int logFileNameLen = strlen(filename);

	if(logFileNameLen <= LOG_FILE_NAME_MAX_LEN)
	{
		memcpy(_logFileFullName, filename, logFileNameLen + 1);
	}
	else
	{
		LOG_E("To big file name %s, got %d, max %d, archiving does not work",
			  filename, logFileNameLen, LOG_FILE_NAME_MAX_LEN);

		_logFileFullName[0] = '\0';
	}

	log_global_set.logger_func = print_to_syslog;
	logger_set_log_level(LOG_MAX_LEVEL_ERROR_WARNING_MESSAGE_DEBUG);

	if(logger_set_log_file(filename) != 0)
		return false;

	// Check and archive log if needed
	if(!logger_archive())
	{
		print_to_syslog(LOG_LEVEL_ERROR, "Unable to archive log file");
		return false;
	}

	return true;
}

/*
 * Deinit loger
 */
void logger_deinit()
{
	cleanup_internal();
}

/*
 * Sel log level
 * @param level - log level
 */
void logger_set_log_level(const int level)
{
	log_global_set.max_log_level = level;
}

/*
 * Set log file name
 * @param filename - log file name
 */
int logger_set_log_file(const char* filename)
{
	cleanup_internal();

	log_global_set.out_file = fopen(filename, "a");

	if (log_global_set.out_file == NULL) {

		syslog(LOG_ERR, "ERROR: File: %s, Line: %d, Fun: %s, Error: Failed to open file %s",
				__FILE__,
				__LINE__,
				__FUNCTION__,
				filename);

		return -1;
	}

	log_global_set.use_stdout  = 0;
	log_global_set.logger_func = print_to_file;

	return 0;
}

/*
 * Set stdout as default
 */
void logger_set_out_stdout()
{
	cleanup_internal();

	log_global_set.use_stdout = 1;
	log_global_set.logger_func = print_to_file;
	log_global_set.out_file = stdout;
}

/*
 * Write buffer to log
 * @param level  - log level
 * @param format - format string
 * @param args   - arguments list
 */
void log_generic(const int level, const char* format, va_list args)
{
	char buffer[256];
	vsprintf(buffer, format, args);
	log_global_set.logger_func(level, buffer);
}

/*
 * Write buffer to log with additions
 * @param level  - log level
 * @param format - format string
 * @param args   - arguments list
 * @param filename - code file name
 * @param line - line in file
 */
void log_generic_add(const int level, const char* format, va_list args, const char* fileName, const char* funName, int line)
{
	char buffer[256];

	sprintf(buffer, "file: %s line: %d fun: %s = ", fileName, line, funName);
	vsprintf(&(buffer[strlen(buffer)]), format, args);
	log_global_set.logger_func(level, buffer);
}

/*
 * Write error log message
 * @param filename - code file name
 * @param line - line in file
 * @param funName - code function name
 * @param format - format string
 */
void log_error(const char* fileName, int line, const char* funName, char* format, ...)
{
	va_list args;
	va_start(args, format);
	log_generic_add(LOG_LEVEL_ERROR, format, args, fileName, funName, line);
	va_end(args);

	_stringCounter++;

	// Check and archive log if needed
	if(_stringCounter >  LOG_STRING_MAX_NUM)
	{
		if(!logger_archive())
			print_to_syslog(LOG_LEVEL_ERROR, "Unable to archive log file");

		_stringCounter = 0;
	}
}

/*
 * Write warning log message
 * @param format - format string
 */
void log_warning(const char* fileName, int line, const char* funName, char* format, ...)
{
	if (log_global_set.max_log_level < LOG_MAX_LEVEL_ERROR_WARNING_MESSAGE) {
		return;
	}

	va_list args;
	va_start(args, format);
	log_generic_add(LOG_LEVEL_WARNING, format, args, fileName, funName, line);
	va_end(args);

	_stringCounter++;

	// Check and archive log if needed
	if(_stringCounter >  LOG_STRING_MAX_NUM)
	{
		if(!logger_archive())
			print_to_syslog(LOG_LEVEL_ERROR, "Unable to archive log file");

		_stringCounter = 0;
	}
}

/*
 * Write simple log message
 * @param format - format string
 */
void log_info(char *format, ...)
{
	if (log_global_set.max_log_level < LOG_MAX_LEVEL_ERROR_WARNING_MESSAGE) {
		return;
	}

	va_list args;
	va_start(args, format);
	log_generic(LOG_LEVEL_MESSAGE, format, args);
	va_end(args);

	_stringCounter++;

	// Check and archive log if needed
	if(_stringCounter >  LOG_STRING_MAX_NUM)
	{
		if(!logger_archive())
			print_to_syslog(LOG_LEVEL_ERROR, "Unable to archive log file");

		_stringCounter = 0;
	}
}

/*
 * Write debug log message
 * @param filename - code file name
 * @param line - line in file
 * @param funName - code function name
 * @param format - format string
 */
void log_debug(char* format, ...)
{
	if (log_global_set.max_log_level <  LOG_MAX_LEVEL_ERROR_WARNING_MESSAGE_DEBUG) {
		return;
	}

	va_list args;
	va_start(args, format);
	log_generic(LOG_LEVEL_DEBUG, format, args);
	va_end(args);

	_stringCounter++;

	// Check and archive log if needed
	if(_stringCounter >  LOG_STRING_MAX_NUM)
	{
		if(!logger_archive())
			print_to_syslog(LOG_LEVEL_ERROR, "Unable to archive log file");

		_stringCounter = 0;
	}
}
