#pragma once

#if defined _WIN32 || defined __CYGWIN__
	#ifdef EXPORT
		#ifdef __GNUC__
			#define DECLSPEC __attribute__ ((dllexport))
		#else
			#define DECLSPEC __declspec(dllexport)
		#endif
	#else
		#ifdef __GNUC__
			#define DECLSPEC __attribute__ ((dllimport))
		#else
			#define DECLSPEC __declspec(dllimport)
		#endif
	#endif
#else
	#if __GNUC__ >= 4
		#define DECLSPEC __attribute__ ((visibility ("default")))
	#else
		#define DECLSPEC
	#endif
#endif

// Introduced for matlab R2014a loadlibrary compatibility with lcc-win32
#ifdef _WIN32
	#ifndef _MSC_VER
		typedef unsigned int uint32_t;
		typedef unsigned short int uint16_t;
		typedef unsigned long int uint64_t;
		typedef signed int int32_t;
	#endif
#endif

#ifdef _WIN32
	#ifndef STD_CALL
		#define CALL_CONV __cdecl
		#define EXTERN extern "C"
	#else
		#define CALL_CONV __stdcall
		#define EXTERN  
	#endif
#else
	#define CALL_CONV
	#define EXTERN extern "C"
#endif

// WARNING: Where used Board is a 0-based index.

// Error codes.
enum error_code {
	DLL_NO_ERROR = 0,								// No errors.
	DLL_MULTI_THREAD_ERROR = 1,						// Another thread has a lock on the library functions.
	DLL_NOT_INITIALIZED = 4,						// DLL is not initialized. Call InitLibrary().
	DLL_CHAR_STRING_SIZE = 5,						// Supplied char buffer size is too short. Return value updated with minimum length.
	DLL_ARRAY_SIZE = 6,								// An array passed as parameter to a function has not enough space to contain all the data that the function should return.
	DLL_ALREADY_INITIALIZED = 42,					// DLL is already initialized.
	DLL_COMM_ERROR = 57,							// Communication error or timeout.
	DLL_ARGUMENT_OUT_OF_RANGE = 60,					// An argument supplied to the library is out of valid range.
	DLL_WRONG_ID = 62,								// The supplied identifier (serial or IP) is not present among the connected systems.
	DLL_TIMEOUT = 64,								// An operation timed out. Result is unspecified.
	DLL_CLOSING = 67,								// Error during library closing.
	DLL_RUNNING = 68,								// The operation cannot be completed while the system is running.
	DLL_WRONG_MODE = 69,							// The function called is not appropriate for the current mode.
	DLL_DECRYPT_FAILED = 71,						// An error occured during decryption.
	DLL_INVALID_BITSTREAM = 72,						// Trying to upload an invalid bistream file.
	DLL_FILE_NOT_FOUND = 73,						// The specified file hasn't been found.
	DLL_INVALID_FIRMWARE = 74,						// Invalid firmware detected on one board. Upload a new one with load_firmware function.
	DLL_UNSUPPORTED_BY_FIRMWARE = 75,				// Function not supported by current firmware of the board. Or firmware on the board is not present or corrupted.
	DLL_THREAD_COMM_ERROR = 76,						// Error during communication between threads.
	DLL_MISSED_SPECTRA = 78,						// One ore more spectra missed.
	DLL_MULTIPLE_INSTANCES = 79,					// Library open by multiple processes.
	DLL_THROUGHPUT_ISSUE = 80,						// Download rate from boards at least 15% lower than expected, check your connection speed.
	DLL_INCOMPLETE_CMD = 81,						// A communication error caused a command to be truncated.
	DLL_MEMORY_FULL = 82,							// The hardware memory became full during the acquisition. Likely the effective throughput is not enough to handle all the data.
	DLL_SLAVE_COMM_ERROR = 83,						// Communication error with slave boards.
	DLL_SOFTWARE_MEMORY_FULL = 84,					// Allowed memory for the DLL became full. Likely some data/event have been discarded.

	// If following errors are returned, contact us for further investigation:
	DLL_WIN32API_FAILED_INIT = 3,					// Win32 errors - Debugging.
	DLL_WIN32API_GET_DEVICE = 7,					// Win32 errors - Debugging.
	DLL_WIN32API = 41,								// Generic WIN32 API error.
	DLL_WIN32API_INIT_EVENTS = 43,					// Win32 errors - Debugging.
	DLL_WIN32API_RD_INIT_EVENTS = 44,				// Win32 errors - Debugging.
	DLL_WIN32API_REPORTED_FAILED_INIT = 45,			// Win32 errors - Debugging - Possible hardware/driver error.
	DLL_WIN32API_UNEXPECTED_FAILED_INIT = 46,		// Win32 errors - Debugging.
	DLL_WIN32API_MULTI_THREAD_INIT_SET = 47,		// Win32 errors - Debugging.
	DLL_WIN32API_LOCK_HMODULE_SET = 48,				// Win32 errors - Debugging.
	DLL_WIN32API_HWIN_SET = 49,						// Win32 errors - Debugging.
	DLL_WIN32API_WMSG_SET = 50,						// Win32 errors - Debugging.
	DLL_WIN32API_READ_SET = 51,						// Win32 errors - Debugging.
	DLL_WIN32API_GET_DEVICE_SIZE = 52,				// Win32 errors - Debugging.
	DLL_WIN32API_DEVICE_UPD_LOCK_G = 53,			// Win32 errors - Debugging.
	DLL_FT_CREATE_IFL = 54,							// FT errors - Debugging.
	DLL_FT_GET_IFL = 55,							// FT errors - Debugging.
	DLL_CREATE_DEVCLASS_RUNTIME = 56,				// Library errors - Debugging - Possible hardware/driver error.
	DLL_CREATE_DEVCLASS_ARGUMENT = 58,				// Library errors - Debugging.
	DLL_CREATE_DEVCLASS_COMM = 59,					// Library errors - Debugging.
	DLL_RUNTIME_ERROR = 61,							// Generic runtime error.	
	DLL_WIN32API_HMODULE = 65,						// Win32 errors - Debugging.
	DLL_WIN32API_DEVICE_UPD_LOCK_F = 66,			// Win32 errors - Debugging.	
	DLL_INIT_EXCEPTION = 77							// Library errors - Debugging.
};

/**  Used to initialize the library. */
EXTERN DECLSPEC bool   CALL_CONV InitLibrary(void);

/**  Used to close the internal library resources (hardware drivers). */
EXTERN DECLSPEC bool   CALL_CONV CloseLibrary(void);

/**  Used to get the last error. */
EXTERN DECLSPEC bool   CALL_CONV getLastError(uint16_t& error_code);

/**  Used to reset "last error" variable to DLL_NO_ERROR. */
EXTERN DECLSPEC bool   CALL_CONV resetLastError(void);

/**  Returns the actual version of the library. */
EXTERN DECLSPEC bool   CALL_CONV libVersion(char* version, uint32_t& version_size);

/** Get number of devices in the query devices (MASTERs ONLY). */
EXTERN DECLSPEC bool   CALL_CONV get_dev_number(uint16_t& devs);

/** Get devices serial from progressive number "nb" of connected devices, "nb" starts from '0'. */
EXTERN DECLSPEC bool   CALL_CONV get_ids(char* identifier, uint16_t& nb, uint16_t& id_size);

// For all the following functions "Board" starts from '0' for the first board (MASTER).

/** To get firmware vesion of a system. */
EXTERN DECLSPEC uint32_t	CALL_CONV getFirmware(const char* identifier, uint16_t Board);

/** Library will return false if not supported by firmware. */
EXTERN DECLSPEC bool   CALL_CONV get_boards_in_chain(const char* identifier, uint16_t& devs);

/**
* Configuration.
* Pass a "configuration" structure to the function to configure the system.
*/

struct configuration {
	uint32_t fast_filter_thr = 0;	// Detection threshold [unit: spectrum BINs]
	uint32_t energy_filter_thr = 0;	// Detection threshold [unit: spectrum BINs]
	uint32_t energy_baseline_thr = 0;	// Energy threshold for baseline [unit: spectrum BINs]
	double max_risetime = 30;	// Max risetime setting for pileup detection 
	double gain = 1;	// Main gain: (spectrum BIN)/(ADC's LSB)
	uint32_t peaking_time = 25;	// Main energy filter peaking time   [unit: 32 ns samples]
	uint32_t max_peaking_time = 0;	// Max energy filter peaking time   [unit: 32 ns samples]
	uint32_t flat_top = 5;	// Main energy filter flat top   [unit: 32 ns samples]
	uint32_t edge_peaking_time = 4;	// Edge detection filter peaking time   [unit: 8 ns samples]
	uint32_t edge_flat_top = 1;	// Edge detection filter flat top   [unit: 8 ns samples]
	uint32_t reset_recovery_time = 300;	// Reset recovery time [unit: 8 ns samples]
	double zero_peak_freq = 1;	// Zero peak rate [kcps]
	uint32_t baseline_samples = 64;	// Baseline samples for baseline correction [32 ns samples]
	bool inverted_input = 0;
	double time_constant = 0;	// Time constant for continuos reset signal
	uint32_t base_offset = 0;	// Baseline of the continous reset signal [ADC's LSB]
	uint32_t overflow_recovery = 0;	// Overflow recovery time [8ns samples]
	uint32_t reset_threshold = 0;	// Reset detection threshold [ADC's LSB]
	double tail_coefficient = 0;	// Tail coefficient
	uint32_t other_param = 0;	
};

struct configuration_offset {
	uint32_t offset_val1;								// First offset value
	uint32_t offset_val2;								// Second offset value.
};

enum GatingMode {
	Disabled,
	EnabledHighLevel,	// Spectrum is acquired only with gating signal at high level
	EnabledLowLevel,	// Spectrum is acquired only with gating signal at low level
	MapLatched			// For each spectrum of map mode, gating value is stored and saved togheter with spectrum.
};

EXTERN DECLSPEC uint32_t   CALL_CONV configure(const char* identifier, uint16_t Board, const configuration cfg);

EXTERN DECLSPEC uint32_t   CALL_CONV configure_gating(const char* identifier, const GatingMode GatingMode);

EXTERN DECLSPEC bool		   CALL_CONV configure_baseline(const char* identifier, uint16_t Board, const uint16_t * baseline_vector);

EXTERN DECLSPEC uint32_t   CALL_CONV configure_offset(const char* identifier, uint16_t Board, const configuration_offset cfg_offset);

EXTERN DECLSPEC uint32_t   CALL_CONV configure_timestamp_delay(const char* identifier, uint16_t Board, int32_t timestamp_delay);

EXTERN DECLSPEC uint32_t   CALL_CONV advanced_configure(const char* identifier, uint16_t Board, uint16_t address, uint32_t data);


/** Used to get the status of the connected systems.
*	Returns true if the "running" parameter is valid.
*	Returns false if an error occours. */
EXTERN DECLSPEC uint32_t   CALL_CONV isRunning_system(const char* identifier, uint16_t Board);

/** To perform a single measure:
* Set time to "0" to get a free-running measure.
* Time is expressed in seconds. */
EXTERN DECLSPEC uint32_t   CALL_CONV start(const char* identifier, const double time, const uint16_t spect_depth);

EXTERN DECLSPEC uint32_t    CALL_CONV stop(const char* identifier);
EXTERN DECLSPEC bool   CALL_CONV clear_chain(const char* identifier);
EXTERN DECLSPEC bool   CALL_CONV clear_board(const char* identifier, uint16_t Board);

/**
* Start acquisition in waveform mode.
* Available modes:
* 0 - Analog acquisition mode.
* 1 - Digital acquisition mode.
* Other settings:
*  - dec_ratio:
*    Decimation ratio of samples: from 1 to 32.
*  - trig_mask:
*    Trigger mask. Set each bit of this integer to '1' to enable the specific trigger.
*    bit 0: Instant trigger
*	  bit 1: Rising slope crossing of trigger level
*	  bit 2: Falling slope crossing of trigger level
*	- trig_level
*	  Trigger level.
*	- length
*	  Waveform length in 2^14 samples units.
*/
EXTERN DECLSPEC uint32_t    CALL_CONV start_waveform(const char* identifier,
	uint16_t mode,
	uint16_t dec_ratio,
	uint32_t trig_mask,
	uint32_t trig_level,
	const double time,
	uint16_t length);

/** To perform an acquisition in list mode:
* Set time to "0" get a free-running measure.
* Time is expressed in seconds. */
EXTERN DECLSPEC uint32_t   CALL_CONV start_list(const char* identifier, const double time);

/** To perform an acquisition in list mode:
* Set time to "0" get a free-running measure.
* Time is expressed in seconds. */
EXTERN DECLSPEC uint32_t   CALL_CONV start_listwave(const char* identifier, const double time, uint16_t dec_ratio, uint16_t length, uint16_t offset);

/** To perform a map:
* Set time to "0" to enable hardware triggering, otherwise it defines the time for each spot.
* Time is expressed in milliseconds. */
EXTERN DECLSPEC uint32_t   CALL_CONV start_map(const char* identifier, const uint32_t sp_time, const uint32_t points, const uint16_t spect_depth);

/** Get available data:
MAPPING MODE: It returns the number of spectra
LIST MODE: number of counts (data). */
EXTERN DECLSPEC bool   CALL_CONV getAvailableData(const char* identifier,
	uint16_t Board,
	uint32_t& data_number
);

// Full statistical data.
struct statistics {
	uint64_t real_time;
	uint64_t live_time;
	double ICR;
	double OCR;
	uint64_t last_timestamp;
	uint32_t detected;
	uint32_t measured;
	uint32_t edge_dt;
	uint32_t filt1_dt;
	uint32_t zerocounts;
	uint32_t baselines_value;
	uint32_t pup_value;
	uint32_t pup_f1_value;
	uint32_t pup_notf1_value;
	uint32_t reset_counter_value;
};

// LIST MODE: 'values' contains all the counts in this format -> timestamp(44 bits) & filter number (4 bits) & converted energy (16 bit);
// Function to be used in normal mode, waveform mode and list mode.

EXTERN DECLSPEC bool   CALL_CONV getData(const char* identifier,
	uint16_t Board,
	uint64_t* values,
	uint32_t& id,
	statistics& stats,
	uint32_t& spectra_size
);

// Get waveform data.
EXTERN DECLSPEC bool   CALL_CONV getListWaveData(const char* identifier,
	uint16_t Board,
	uint64_t* values,
	uint64_t* wave_values,
	uint32_t& id,
	statistics& stats,
	uint32_t& spectra_size
);

// Get waveform data.
EXTERN DECLSPEC bool   CALL_CONV getWaveData(const char* identifier,
	uint16_t Board,
	uint16_t* values,
	uint32_t& data_size
);


/** Get spectra data (to be used only in mapping mode).
'values' contains all the single spectra one after the other;
'id' their progressive id;
'stats' their statistics one after the others in this order: real_time (useconds), live_time (useconds), ICR (kcps), OCR (kcps).
'advstats' an array of 18 elements per spectra containing advanced statistics.
*/
EXTERN DECLSPEC bool   CALL_CONV getAllData(const char* identifier,
	uint16_t Board,
	uint16_t* values,
	uint32_t* id,
	double* stats,
	uint64_t* advstats,
	uint32_t& spectra_size,
	uint32_t& data_number
);

//last spectrum is copied in array 
EXTERN DECLSPEC bool   CALL_CONV getLiveDataMap(const char* identifier,
	uint16_t Board,
	uint16_t* values,
	uint32_t& id,
	statistics& stats,
	uint32_t& spectra_size
);

/*
Register callback function for each packet.
*/
EXTERN DECLSPEC bool   CALL_CONV register_callback(void (*callback)(uint16_t type, uint32_t call_id, uint32_t length, uint32_t* data));

/*
Add board to query.
*/
EXTERN DECLSPEC bool   CALL_CONV add_to_query(char* address);

/*
Remove board from query.
*/
EXTERN DECLSPEC bool   CALL_CONV remove_from_query(char* address);

/*
Write a new IP configuration. Changes will take effect after the board has been rebooted.
*/
EXTERN DECLSPEC uint32_t CALL_CONV write_IP_configuration(const char* identifier, char* IP, char* subnet_mask, char* gateway);

/*
Loads a new firmware via USB. With store to false, it will load to the system a temporary firmware (20 seconds) that will be lost if rebooted.
Otherwise whith store to true it will store the firmware on memory and it will persist even after power cycles.
This last operation could take up to 40 seconds.
The filename must contain the path in this format: C:\\ArbitraryDirectory\\firmware_name.bitc
*/
EXTERN DECLSPEC uint32_t  CALL_CONV load_firmware(const char* identifier, bool store, char* filename);

// Resets the communication on the entire chain.
EXTERN DECLSPEC uint32_t  CALL_CONV global_reset(const char* identifier);

// Get the progress of the load firmware operation (floating number from 0 to 1);
EXTERN DECLSPEC bool  CALL_CONV get_load_fw_progress(double& progress);

