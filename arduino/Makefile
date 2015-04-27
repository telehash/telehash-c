# I'd love help making this more automagical xplatform

#ARDUINO=arduino
ARDUINO=/Users/jer/Downloads/Arduino-5.app/Contents/MacOS/JavaApplicationStub

#PORT=/dev/ttyACM0
PORT=/dev/tty.usbmodem1431
BOARD=pinoccio:avr:pinoccio
SKETCH=

all: test

test: SKETCH = $(CURDIR)/examples/Test/Test.ino
test: upload

slink: SKETCH=$(CURDIR)/examples/SLink/SLink.ino
slink: upload

upload:
	mkdir -p $(CURDIR)/build
	${ARDUINO} --upload --board ${BOARD} --port ${PORT} --pref build.path=$(CURDIR)/build ${SKETCH}

