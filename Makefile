LDFLAGS=-lcurl
CFLAGS=-O0 -g -Wall -Wextra

all: notifier cacert.pem

release: release-wpa_supplicant

release-iwd: clean
	$(MAKE) CFLAGS="-O2 -s -DWPA_SUPPLICANT=0 -DIWD=1 -DDEBUGGING=0" all

release-wpa_supplicant: clean
	$(MAKE) CFLAGS="-O2 -s -DWPA_SUPPLICANT=1 -DIWD=0 -DDEBUGGING=0" all

cacert.pem:
	curl -O https://curl.se/ca/cacert.pem

test: notifier
	<sample ./notifier

clean:
	rm -f notifier

.PHONY: release-iwd release-wpa_supplicant test clean
