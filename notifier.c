#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <curl/curl.h>

#ifndef DEBUGGING
#	define DEBUGGING 1 /* set to 1 to print failures and results. don't do this in production unless you don't mind inetd sending the error back over the tcp connection! */
#endif
#ifndef WPA_SUPPLICANT
#	define WPA_SUPPLICANT 0 /* 1=set WiFi SSID/password for wpa_supplicant 0=disabled*/
#endif
#ifndef IWD
#	define IWD 0 /* 1=set WiFi SSID/password for iwd 0=disabled */
#endif

#include "secrets.h"

char *email_buffer;
size_t payload_source(char*, size_t, size_t, void*);
struct upload_status {
  size_t bytes_read;
} upload_ctx = {0};

int main()
{
	// NOTE!
	// NOTE! Field order must be correct for this code to work! It was probably simpler to make the field order not matter but this way it saves a couple of CPU cycles.
	// NOTE!
	//
	// NOTE! For wpa_supplicant, all old WiFi SSID/password information is overwritten. Only the most recently set SSID/password will be used!
	// NOTE! For iwd, it is DIFFERENT. A new password with the same SSID will overwrite that password, but all old SSID/password combos are untouched!

	// Linux allows 128-byte WPA3 passphrases
	// Let's account for up to 4x 128-byte email addresses
	// Separators sum up to approx 128 bytes.
	// Let's give 128 each for Body and Subject and SSID. (x3)
	// So 9x128 should be enough for now. Round it up to 1536 (1.5kb) 'cause round numbers.
	// (ya don't put this on the public internet...buffer overflow time!)
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

	if(password_length > 0)
	{
		if(DEBUGGING)
			printf("Setting WiFi SSID and password for:");
		if(WPA_SUPPLICANT)
		{
			if(DEBUGGING)
				printf(" wpa_supplicant");
			char *wpa_file = "/etc/wpa_supplicant/wpa_supplicant.conf";
			FILE *wpa = fopen(wpa_file,"w");
			if(!wpa)
			{
				if(DEBUGGING)
					printf("Failed to open file %s: %s\n", wpa_file, strerror(errno));
				exit(EXIT_FAILURE);
			}
			fprintf(wpa, "ctrl_interface=DIR=/var/run/wpa_supplicant GROUP=netdev\n");
			fprintf(wpa, "update_config=1\n");
			fprintf(wpa, "country=US\n");
			fprintf(wpa, "p2p_disabled=1\n");
			fprintf(wpa, "network={\n");
			fprintf(wpa, "ssid=\"%s\"\n", ssid);
			fprintf(wpa, "psk=\"%s\"\n", password);
			fprintf(wpa, "}\n");
			fclose(wpa);
			system("wpa_cli reconfigure"); // tell wpa_supplicant to reload the password
		}
		else if(IWD)
		{
			if(DEBUGGING)
				printf(" iwd");
			char *iwd_path = "/var/lib/iwd/";
			char *iwd_suffix = ".psk";
			char *iwd_file = malloc(strlen(ssid) + strlen(iwd_path) + strlen(iwd_suffix) + 1);
			iwd_file[0] = 0;
			strcat(iwd_file, iwd_path);
			strcat(iwd_file, ssid);
			strcat(iwd_file, iwd_suffix);
			FILE *iwd = fopen(iwd_file, "w");
			if(!iwd)
			{
				if(DEBUGGING)
					printf("Failed to open file %s: %s\n", iwd_file, strerror(errno));
			}
			free(iwd_file);
			fprintf(iwd, "[Security]\n");
			fprintf(iwd, "Passphrase=%s\n", password);
			fclose(iwd);
			// iwd reloads the password automatically, according to the docs
		}
		else
		{
			if(DEBUGGING)
				printf(" (none)");
		}
		printf("\n");
	}

	if(subject_length > 0 && email1_length > 0)
	{
		if(DEBUGGING)
			printf("Sending email\n");
		CURL *curl;
		CURLcode res = CURLE_OK;
		struct curl_slist *recipients = NULL;
		curl = curl_easy_init();
		if(!curl)
		{
			if(DEBUGGING)
				printf("loading curl failed\n");
			exit(EXIT_FAILURE);
		}
		curl_easy_setopt(curl, CURLOPT_USERNAME, FROM_ADDRESS);
		curl_easy_setopt(curl, CURLOPT_PASSWORD, MAIL_PASSWORD);
		curl_easy_setopt(curl, CURLOPT_URL, MAIL_URL);
		curl_easy_setopt(curl, CURLOPT_USE_SSL, (long)CURLUSESSL_ALL);
		curl_easy_setopt(curl, CURLOPT_CAINFO, CA_INFO_PATH);
		curl_easy_setopt(curl, CURLOPT_MAIL_FROM, FROM_ADDRESS);

		size_t ebb_len = email1_length + email2_length + email3_length + strlen(FROM_ADDRESS) + strlen(FROM_NAME)
			+ cc_length + subject_length + body_length + 250; // should need no more than 36 extra characters if I did my math right. I don't trust my math...
		email_buffer = malloc(ebb_len);
		char *email_tmp = email_buffer;
		if(!email_buffer)
		{
			if(DEBUGGING)
				printf("failed allocationg email buffer\n");
			exit(EXIT_FAILURE);
		}

		email_tmp += sprintf(email_tmp, "From: %s <%s>\r\n", FROM_NAME, FROM_ADDRESS);
		email_tmp += sprintf(email_tmp, "To: <%s>", email1);
		recipients = curl_slist_append(recipients, email1);
		if(email2_length)
		{
			email_tmp += sprintf(email_tmp, ",<%s>", email2);
			recipients = curl_slist_append(recipients, email2);
		}
		if(email3_length)
		{
			email_tmp += sprintf(email_tmp, ",<%s>", email3);
			recipients = curl_slist_append(recipients, email3);
		}
		email_tmp += sprintf(email_tmp, "\r\n");
		if(cc_length)
		{
			email_tmp += sprintf(email_tmp, "Cc: <%s>\r\n", cc);
			recipients = curl_slist_append(recipients, cc);
		}
		email_tmp += sprintf(email_tmp, "Subject: %s\r\n\r\n", subject);
		// TODO FIXME does the PLC send \r\n line endings in the body? or are there no newlines at all?
		email_tmp += sprintf(email_tmp, "%s\r\n", body);

		if(DEBUGGING)
			printf("\nEmail ready to send:\n\n%s", email_buffer);

		curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);
		curl_easy_setopt(curl, CURLOPT_READFUNCTION, payload_source);
		curl_easy_setopt(curl, CURLOPT_READDATA, &upload_ctx);
		curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
		if(DEBUGGING)
			curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
		res = curl_easy_perform(curl);

		free(email_buffer);

		if(res != CURLE_OK)
		{
			if(DEBUGGING)
				printf("Failed to send email: %s\n", curl_easy_strerror(res));
			exit(EXIT_FAILURE);
		}
		curl_slist_free_all(recipients);
		curl_easy_cleanup(curl);
	}

	exit(EXIT_SUCCESS);
}

// copied from libcurl docs example
size_t payload_source(char *ptr, size_t size, size_t nmemb, void *userp)
{
	struct upload_status *upload_ctx = (struct upload_status *)userp;
	const char *data;
	size_t room = size * nmemb;

	if((size == 0) || (nmemb == 0) || ((size*nmemb) < 1))
	{
		return 0;
	}

	data = &email_buffer[upload_ctx->bytes_read];

	if(data)
	{
		size_t len = strlen(data);
		if(room < len)
			len = room;
		memcpy(ptr, data, len);
		upload_ctx->bytes_read += len;

		return len;
	}

	return 0;
}
