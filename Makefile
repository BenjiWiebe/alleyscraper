LDFLAGS=-lcurl

all: notifier cacert.pem

cacert.pem:
	curl -O https://curl.se/ca/cacert.pem

test: notifier
	<sample ./notifier
