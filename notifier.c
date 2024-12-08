#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#define DEBUGGING 1 /* set to 1 to print failures and results. don't do this in production unless you don't mind inetd sending the error back over the tcp connection! */
int main()
{
	// NOTE!
	// NOTE! Field order must be correct for this code to work! It was probably simpler to make the field order not matter but this way it saves a couple of CPU cycles.
	// NOTE!

	// Linux allows 128-byte WPA3 passphrases
	// Let's account for up to 4x 128-byte email addresses
	// Separators sum up to approx 128 bytes.
	// Let's give 128 each for Body and Subject and SSID. (x3)
	// So 9x128 should be enough for now. Round it up to 1536 (1.5kb) 'cause round numbers.
	char buf[1536] = {0};
	char *bufp = buf;
	ssize_t bytes_read = read(STDIN_FILENO, buf, sizeof(buf));
	buf[bytes_read] = 0; // null terminate it
	if(!bytes_read)
		exit(EXIT_SUCCESS);
	

	/*
	 * This amazing lump of preprocessor finds the beginning and end of a field, as delimited by token/tokenend for start and end,
	 *     and also increments the buffer to skip over the field, ready for subsequent calls.
	 *
	 * RETURNS:
	 * It gives us a pointer to that field in the buffer (and null-terminates the field in-situ) named varname as well as a length field, named varname_length.
	 *
	 * ERRORS:
	 * If the first token is not found, it modifies nothing and optionally (if DEBUGGING) logs the failure.
	 *     HOWEVER, it does still initialize varname/varname_length to an empty string and 0, respectively.
	 * If the first token is present but the second token is not found, it exits the program with a failure return code.
	 */
	#define extract(buffer, token, tokenend, varname) \
		char *varname = ""; int varname ## _length = 0; /* declare the variables we want to be accessible to the rest of the program */ \
		char *varname ## _start = strstr(buffer, token); /* get pointer to start of first token */ \
		if(!varname ## _start) { if(DEBUGGING){printf("could not find token '%s'\n",token);} } else { /* field doesn't exist - skip the rest of the code, log if DEBUGGING. if field exists, continue */ \
		buffer = varname ## _start; /* set buffer to start of first token */ \
		buffer += strlen(token); /* advance buffer beyond first token */ \
		char *varname ## _end = strstr(buffer, tokenend); /* get pointer to start of second token */ \
		if(!varname ## _end) { if(DEBUGGING){printf("could not find token '%s'\n",tokenend);}exit(EXIT_FAILURE); } /* exit (error) if not found, log if DEBUGGING */ \
		varname ## _length = varname ## _end - buffer; /* get length of field by pointer math: start of end token minus current buffer point (still at start of field) */ \
		buffer[varname ## _length] = 0; /* null-terminate by nulling the first character of the end token i.e. one past the end of the field */ \
		varname = buffer; /* varname can now be set to our current buffer pointer, since it got null-terminated */ \
		buffer += varname ## _length; /* advance buffer beyond the current field */ \
		buffer += strlen(tokenend); } /* advance buffer beyond the end token. end else (if first token was found / field exists) */ \
	
	extract(bufp, "To :", "ToEnd:", email1);
	extract(bufp, "Cc :", "CcEnd:", cc);
	extract(bufp, "Subject:", "SubjectEnd:", subject);
	extract(bufp, "Body :", "BodyEnd:", body);
	extract(bufp, "SSID :", "SSIDEnd:", ssid);
	extract(bufp, "Password :", "PasswordEnd:", password);
	extract(bufp, "To2:", "To2End: ", email2);
	extract(bufp, "To3:", "To3End: ", email3);
	
	if(DEBUGGING)
		printf("email1 = %s\ncc = %s\nsubject = %s\nbody = %s\nssid = %s\npassword = %s\nemail2 = %s\nemail3 = %s\n", email1, cc, subject, body, ssid, password, email2, email3);

	exit(EXIT_SUCCESS);
}
