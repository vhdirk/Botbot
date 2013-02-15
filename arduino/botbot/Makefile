########################################################################
#
# Arduino command line tools Makefile
# 
########################################################################

ARDUINO_DIR  := /usr/share/arduino
ARDUINO_SKETCHBOOK := $(HOME)/development/arduino/Botbot/arduino


TARGET       := botbot
ARDUINO_LIBS := Time Ethernet Ethernet/utility aJson SPI Wire Wire/utility

BOARD_TAG    = mega2560
ARDUINO_PORT = /dev/ttyACM0

# forked this from 
include $(ARDUINO_SKETCHBOOK)/Arduino.mk

# You might also want to specify these, but normally they'll be read from the
# boards.txt file i.e. implied by BOARD_TAG
#
#    MCU,F_CPU    - The target processor description
#
# Once this file has been created the typical workflow is just
#
#   $ make upload
#
# All of the object files are created in the build-cli subdirectory
# All sources should be in the current directory and can include:
#  - at most one .pde or .ino file which will be treated as C++ after
#    the standard Arduino header and footer have been affixed.
#  - any number of .c, .cpp, .s and .h files
#
# Included libraries are built in the build-cli/libs subdirectory.
#
# Besides make upload you can also
#   make             - no upload
#   make clean       - remove all our dependencies
#   make depends     - update dependencies
#   make reset       - reset the Arduino by tickling DTR on the serial port
#   make raw_upload  - upload without first resetting
#   make show_boards - list all the boards defined in boards.txt

